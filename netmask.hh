#pragma once
#include "swrappers.hh"
#include <boost/utility.hpp>
#include <memory>
#include <string>
#include <bitset>
#include <vector>
#include <sstream>

using std::unique_ptr;
using std::string;
using std::vector;
using std::ostringstream;

/** Per-bit binary tree map implementation with <Netmask,T> pair.
 *
 * This is an binary tree implementation for storing attributes for IPv4 and IPv6 prefixes.
 * The most simple use case is simple NetmaskTree<bool> used by NetmaskGroup, which only
 * wants to know if given IP address is matched in the prefixes stored.
 *
 * This element is useful for anything that needs to *STORE* prefixes, and *MATCH* IP addresses
 * to a *LIST* of *PREFIXES*. Not the other way round.
 *
 * You can store IPv4 and IPv6 addresses to same tree, separate payload storage is kept per AFI.
 *
 * To erase something copy values to new tree sans the value you want to erase.
 *
 * Use swap if you need to move the tree to another NetmaskTree instance, it is WAY faster
 * than using copy ctor or assignment operator, since it moves the nodes and tree root to
 * new home instead of actually recreating the tree.
 *
 * Please see NetmaskGroup for example of simple use case. Other usecases can be found
 * from GeoIPBackend and Sortlist, and from dnsdist.
 */
template <typename T>
class NetmaskTree {
public:
  typedef Netmask key_type;
  typedef T value_type;
  typedef std::pair<key_type,value_type> node_type;
  typedef size_t size_type;

private:
  /** Single node in tree, internal use only.
    */
  class TreeNode : boost::noncopyable {
  public:
     explicit TreeNode(int bits) noexcept : parent(NULL),d_bits(bits) {
     }

     //<! Makes a left node with one more bit than parent
     TreeNode* make_left() {
       if (!left) {
         left = unique_ptr<TreeNode>(new TreeNode(d_bits+1));
         left->parent = this;
       }
       return left.get();
     }

     //<! Makes a right node with one more bit than parent
     TreeNode* make_right() {
       if (!right) {
         right = unique_ptr<TreeNode>(new TreeNode(d_bits+1));
         right->parent = this;
       }
       return right.get();
     }

     unique_ptr<TreeNode> left;
     unique_ptr<TreeNode> right;
     TreeNode* parent;

     unique_ptr<node_type> node4; //<! IPv4 value-pair
     unique_ptr<node_type> node6; //<! IPv6 value-pair

     int d_bits; //<! How many bits have been used so far
  };

public:
  NetmaskTree() noexcept : NetmaskTree(false) {
  }

  NetmaskTree(bool cleanup) noexcept : d_cleanup_tree(cleanup) {
  }

  NetmaskTree(const NetmaskTree& rhs): d_cleanup_tree(rhs.d_cleanup_tree) {
    // it is easier to copy the nodes than tree.
    // also acts as handy compactor
    for(auto const& node: rhs._nodes)
      insert(node->first).second = node->second;
  }

  NetmaskTree& operator=(const NetmaskTree& rhs) {
    clear();
    // see above.
    for(auto const& node: rhs._nodes)
      insert(node->first).second = node->second;
    d_cleanup_tree = rhs.d_cleanup_tree;
    return *this;
  }

  const typename std::vector<node_type*>::const_iterator begin() const { return _nodes.begin(); }
  const typename std::vector<node_type*>::const_iterator end() const { return _nodes.end(); }

  typename std::vector<node_type*>::iterator begin() { return _nodes.begin(); }
  typename std::vector<node_type*>::iterator end() { return _nodes.end(); }

  node_type& insert(const string &mask) {
    return insert(key_type(mask));
  }

  //<! Creates new value-pair in tree and returns it.
  node_type& insert(const key_type& key) {
    // lazily initialize tree on first insert.
    if (!root) root = unique_ptr<TreeNode>(new TreeNode(0));
    TreeNode* node = root.get();
    node_type* value = nullptr;

    if (key.getNetwork().sin4.sin_family == AF_INET) {
      std::bitset<32> addr(be32toh(key.getNetwork().sin4.sin_addr.s_addr));
      int bits = 0;
      // we turn left on 0 and right on 1
      while(bits < key.getBits()) {
        uint8_t val = addr[31-bits];
        if (val)
          node = node->make_right();
        else
          node = node->make_left();
        bits++;
      }
      // only create node if not yet assigned
      if (!node->node4) {
        node->node4 = unique_ptr<node_type>(new node_type());
        _nodes.push_back(node->node4.get());
      }
      value = node->node4.get();
    } else {
      uint64_t* addr = (uint64_t*)key.getNetwork().sin6.sin6_addr.s6_addr;
      std::bitset<64> addr_low(be64toh(addr[1]));
      std::bitset<64> addr_high(be64toh(addr[0]));
      int bits = 0;
      while(bits < key.getBits()) {
        uint8_t val;
        // we use high address until we are
        if (bits < 64) val = addr_high[63-bits];
        // past 64 bits, and start using low address
        else val = addr_low[127-bits];

        // we turn left on 0 and right on 1
        if (val)
          node = node->make_right();
        else
          node = node->make_left();
        bits++;
      }
      // only create node if not yet assigned
      if (!node->node6) {
        node->node6 = unique_ptr<node_type>(new node_type());
        _nodes.push_back(node->node6.get());
      }
      value = node->node6.get();
    }
    // assign key
    value->first = key;
    return *value;
  }

  //<! Creates or updates value
  void insert_or_assign(const key_type& mask, const value_type& value) {
    insert(mask).second = value;
  }

  void insert_or_assign(const string& mask, const value_type& value) {
    insert(key_type(mask)).second = value;
  }

  //<! check if given key is present in TreeMap
  bool has_key(const key_type& key) const {
    const node_type *ptr = lookup(key);
    return ptr && ptr->first == key;
  }

  //<! Returns "best match" for key_type, which might not be value
  const node_type* lookup(const key_type& value) const {
    return lookup(value.getNetwork(), value.getBits());
  }

  //<! Perform best match lookup for value, using at most max_bits
  const node_type* lookup(const ComboAddress& value, int max_bits = 128) const {
    if (!root) return nullptr;

    TreeNode *node = root.get();
    node_type *ret = nullptr;

    // exact same thing as above, except
    if (value.sin4.sin_family == AF_INET) {
      max_bits = std::max(0,std::min(max_bits,32));
      std::bitset<32> addr(be32toh(value.sin4.sin_addr.s_addr));
      int bits = 0;

      while(bits < max_bits) {
        // ...we keep track of last non-empty node
        if (node->node4) ret = node->node4.get();
        uint8_t val = addr[31-bits];
        // ...and we don't create left/right hand
        if (val) {
          if (node->right) node = node->right.get();
          // ..and we break when road ends
          else break;
        } else {
          if (node->left) node = node->left.get();
          else break;
        }
        bits++;
      }
      // needed if we did not find one in loop
      if (node->node4) ret = node->node4.get();
    } else {
      uint64_t* addr = (uint64_t*)value.sin6.sin6_addr.s6_addr;
      max_bits = std::max(0,std::min(max_bits,128));
      std::bitset<64> addr_low(be64toh(addr[1]));
      std::bitset<64> addr_high(be64toh(addr[0]));
      int bits = 0;
      while(bits < max_bits) {
        if (node->node6) ret = node->node6.get();
        uint8_t val;
        if (bits < 64) val = addr_high[63-bits];
        else val = addr_low[127-bits];
        if (val) {
          if (node->right) node = node->right.get();
          else break;
        } else {
          if (node->left) node = node->left.get();
          else break;
        }
        bits++;
      }
      if (node->node6) ret = node->node6.get();
    }

    // this can be nullptr.
    return ret;
  }

  void cleanup_tree(TreeNode* node)
  {
    // only cleanup this node if it has no children and node4 and node6 are both empty
    if (!(node->left || node->right || node->node6 || node->node4)) {
      // get parent node ptr
      TreeNode* parent = node->parent;
      // delete this node
      if (parent) {
	if (parent->left.get() == node)
	  parent->left.reset();
	else
	  parent->right.reset();
	// now recurse up to the parent
	cleanup_tree(parent);
      }
    }
  }

  //<! Removes key from TreeMap. This does not clean up the tree.
  void erase(const key_type& key) {
    TreeNode *node = root.get();

    // no tree, no value
    if ( node == nullptr ) return;

    // exact same thing as above, except
    if (key.getNetwork().sin4.sin_family == AF_INET) {
      std::bitset<32> addr(be32toh(key.getNetwork().sin4.sin_addr.s_addr));
      int bits = 0;
      while(node && bits < key.getBits()) {
        uint8_t val = addr[31-bits];
        if (val) {
          node = node->right.get();
        } else {
          node = node->left.get();
        }
        bits++;
      }
      if (node) {
        for(auto it = _nodes.begin(); it != _nodes.end(); ) {
          if (node->node4.get() == *it)
            it = _nodes.erase(it);
          else
            it++;
        }

        node->node4.reset();

        if (d_cleanup_tree)
          cleanup_tree(node);
      }
    } else {
      uint64_t* addr = (uint64_t*)key.getNetwork().sin6.sin6_addr.s6_addr;
      std::bitset<64> addr_low(be64toh(addr[1]));
      std::bitset<64> addr_high(be64toh(addr[0]));
      int bits = 0;
      while(node && bits < key.getBits()) {
        uint8_t val;
        if (bits < 64) val = addr_high[63-bits];
        else val = addr_low[127-bits];
        if (val) {
          node = node->right.get();
        } else {
          node = node->left.get();
        }
        bits++;
      }
      if (node) {
        for(auto it = _nodes.begin(); it != _nodes.end(); ) {
          if (node->node6.get() == *it)
            it = _nodes.erase(it);
          else
            it++;
        }

        node->node6.reset();

        if (d_cleanup_tree)
          cleanup_tree(node);
      }
    }
  }

  void erase(const string& key) {
    erase(key_type(key));
  }

  //<! checks whether the container is empty.
  bool empty() const {
    return _nodes.empty();
  }

  //<! returns the number of elements
  size_type size() const {
    return _nodes.size();
  }

  //<! See if given ComboAddress matches any prefix
  bool match(const ComboAddress& value) const {
    return (lookup(value) != nullptr);
  }

  bool match(const std::string& value) const {
    return match(ComboAddress(value));
  }

  //<! Clean out the tree
  void clear() {
    _nodes.clear();
    root.reset(nullptr);
  }

  //<! swaps the contents, rhs is left with nullptr.
  void swap(NetmaskTree& rhs) {
    root.swap(rhs.root);
    _nodes.swap(rhs._nodes);
  }

private:
  unique_ptr<TreeNode> root; //<! Root of our tree
  std::vector<node_type*> _nodes; //<! Container for actual values
  bool d_cleanup_tree; //<! Whether or not to cleanup the tree on erase
};

/** This class represents a group of supplemental Netmask classes. An IP address matchs
    if it is matched by zero or more of the Netmask classes within.
*/
class NetmaskGroup
{
public:
  //! By default, initialise the tree to cleanup
  NetmaskGroup() noexcept : NetmaskGroup(true) {
  }

  //! This allows control over whether to cleanup or not
  NetmaskGroup(bool cleanup) noexcept : tree(cleanup) {
  }

  //! If this IP address is matched by any of the classes within

  bool match(const ComboAddress *ip) const
  {
    const auto &ret = tree.lookup(*ip);
    if(ret) return ret->second;
    return false;
  }

  bool match(const ComboAddress& ip) const
  {
    return match(&ip);
  }

  bool lookup(const ComboAddress* ip, Netmask* nmp) const
  {
    const auto &ret = tree.lookup(*ip);
    if (ret) {
      if (nmp != nullptr)
        *nmp = ret->first;

      return ret->second;
    }
    return false;
  }

  bool lookup(const ComboAddress& ip, Netmask* nmp) const
  {
    return lookup(&ip, nmp);
  }

  //! Add this string to the list of possible matches
  void addMask(const string &ip, bool positive=true)
  {
    if(!ip.empty() && ip[0] == '!') {
      addMask(Netmask(ip.substr(1)), false);
    } else {
      addMask(Netmask(ip), positive);
    }
  }

  //! Add this Netmask to the list of possible matches
  void addMask(const Netmask& nm, bool positive=true)
  {
    tree.insert(nm).second=positive;
  }

  //! Delete this Netmask from the list of possible matches
  void deleteMask(const Netmask& nm)
  {
    tree.erase(nm);
  }

  void deleteMask(const std::string& ip)
  {
    if (!ip.empty())
      deleteMask(Netmask(ip));
  }

  void clear()
  {
    tree.clear();
  }

  bool empty() const
  {
    return tree.empty();
  }

  size_t size() const
  {
    return tree.size();
  }

  string toString() const
  {
    ostringstream str;
    for(auto iter = tree.begin(); iter != tree.end(); ++iter) {
      if(iter != tree.begin())
        str <<", ";
      if(!((*iter)->second))
        str<<"!";
      str<<(*iter)->first.toString();
    }
    return str.str();
  }

  void toStringVector(vector<string>* vec) const
  {
    for(auto iter = tree.begin(); iter != tree.end(); ++iter) {
      vec->push_back(((*iter)->second ? "" : "!") + (*iter)->first.toString());
    }
  }
#if 0
  void toMasks(const string &ips)
  {
    vector<string> parts;
    stringtok(parts, ips, ", \t");

    for (vector<string>::const_iterator iter = parts.begin(); iter != parts.end(); ++iter)
      addMask(*iter);
  }
#endif
private:
  NetmaskTree<bool> tree;
};

