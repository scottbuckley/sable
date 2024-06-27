#include <iterator>
#include <functional>
#include <iostream>
using namespace std;

class btree {
private:
  int rowid = -1;
public:
  shared_ptr<btree> bfalse;
  shared_ptr<btree> btrue;

  btree(unsigned d) {
    rowid = d;
  }
  btree() {}

  unsigned get_rowid() {
    if (rowid == -1) throw logic_error("trying to access the rowid that doesn't yet exist");
    return rowid;
  }

  bool has_rowid() {
    return (rowid != -1);
  }

  void set_rowid(unsigned data) {
    this->rowid = data;
  }

  bool is_leaf() {
    return !(btrue || bfalse);
  };

  void print(ostream & out = cout, string prefix = "") {
    if (is_leaf()) {
      out << prefix << "Leaf: " << rowid << endl;
    } else {
      if (btrue) {
        out << prefix << "True:" << endl;
        btrue->print(out, prefix + ".  ");
      }
      if (bfalse) {
        out << prefix << "False:" << endl;
        bfalse->print(out, prefix + ".  ");
      }
    }
  }

  void print_table(ostream & out = cout, unsigned labelwidth = 3) {
    auto it_end = end();
    for (BTIterator it = begin(); it != it_end; ++it) {
      string labelnumber = to_string(*it);
      string label = pad_number(*it, labelwidth);
      out << label << ": ";
      auto path = it.get_bt_path();
      for (auto b : path)
        out << b << " ";
      out << endl;
    }
  }

  /* Return the 'true' child of this node, creating it if necessary. */
  shared_ptr<btree> force_true() {
    if (btrue) return btrue;
    btrue = make_shared<btree>();
    return btrue;
  }

  /* Return the 'false' child of this node, creating it if necessary. */
  shared_ptr<btree> force_false() {
    if (bfalse) return bfalse;
    bfalse = make_shared<btree>();
    return bfalse;
  }

  /* Return the 'true' or 'false' child of this node, creating it if necessary. */
  shared_ptr<btree> force_bool(bool b) {
    if (b) return force_true();
    else   return force_false();
  }

  typedef std::vector<bool> bpath;
  typedef std::vector<btree *> bstack;

  void add_at_path(bpath path, unsigned data) {
    btree * cur_node = this;
    for (bool b : path) {
      if (b) cur_node = cur_node->force_true().get();
      else   cur_node = cur_node->force_false().get();
    }
    cur_node->rowid = data;
  };

  struct BTIterator {
  private:
    bpath bt_path;
    bstack bt_stack;
  public:

    bpath get_bt_path() {
      return bt_path;
    }

    // assumes that we don't start with an empty stack
    void find_first() {
      while (true) {
        if (bt_stack.back()->btrue) {
          bt_path.push_back(true);
          bt_stack.push_back(bt_stack.back()->btrue.get());
        } else if (bt_stack.back()->bfalse) {
          bt_path.push_back(false);
          bt_stack.push_back(bt_stack.back()->bfalse.get());
        } else {
          break;
        }
      }
    }

    BTIterator(bpath path, bstack stack) {
      bt_path = path;
      bt_stack = stack;
    };
    
    ~BTIterator() {};

    BTIterator(btree * root, const bool move_to_first = true) {
      bt_stack.push_back(root);
      if (move_to_first) find_first();
    }

    // static BTIterator begin(bpath & path, bstack & stack) {
    //   while (true) {
    //     if (stack.back()->btrue) {
    //       path.push_back(true);
    //       stack.push_back(stack.back()->btrue);
    //     } else if (stack.back()->bfalse) {
    //       path.push_back(false);
    //       stack.push_back(stack.back()->bfalse);
    //     } else {
    //       break;
    //     }
    //   }
    //   return BTIterator(path, stack);
    // };

    void dump() {
      cout << "path: ";
      for (auto b : bt_path)
        cout << b;
      cout << " stacklen: " << bt_stack.size() << " rowid: " << bt_stack.back()->get_rowid() << endl;
    };

    // note: TRUE is on the LEFT.
    // we explore TRUE before exploring FALSE
    BTIterator operator++() {
      // cout << "calling ++" << endl;
      // dump();
      if (bt_path.empty()) return *this;
      // go up while we are coming from "false" or if coming
      // from "true" but there is no "false" branch to follow.

      // from here, the size of the path and stack will be the same, but
      // when we return, the stack should always be one longer than the path.
      // maybe i can enforce this in the type somehow
      bt_stack.pop_back();

      // const bool path_end = bt_path.back();
      
      if (bt_stack.empty()) throw std::logic_error("oh noez");
      //  "we are coming from the right" || "we came from the left but there's nothing on the right"
      while (bt_path.back() == false || !(bt_stack.back()->bfalse)) {
        // cout << "iterating" << endl;
        // print_vector(bt_path);
        // print_vector(bt_stack);
        if (bt_stack.empty()) throw std::logic_error("oh noez");
        if (bt_path.size() == 1) {
          // we just got to the top
          if (bt_path.back() == false) {
            // we got here from the right, which means we are finished. we want to return "end",
            // which is the empty path and the root node.
            bt_path.pop_back();
            return *this;
          }
          // we got here from the left
          if (bt_stack.back()->bfalse) {
            // we can go to the right
            // replace the end of the path with "false"
            bt_path.pop_back();
            bt_path.push_back(false);
            // add the right path to the end of the stack, and then "find_first" to get its first leaf
            bt_stack.push_back(bt_stack.back()->bfalse.get());
            find_first();
            return *this;
          }
          // we can't go to the right, so we are at the end.
          bt_path.pop_back();
          return *this;
        }
        // otherwise, we just move up one
        bt_path.pop_back();
        bt_stack.pop_back();
        // if (bt_path.size() == 1 && bt_path.back() == true) {
        //   // we just returned to the root, we are done.
        //   bt_path.pop_back();
        //   return BTIterator(bt_path, bt_stack);
        // }
        // bt_path.pop_back();
        // bt_stack.pop_back();
      }
      // we got here, which means we can now go down the right-hand path
      bt_path.pop_back();
      bt_path.push_back(false);
      bt_stack.push_back(bt_stack.back()->bfalse.get());
      find_first();
      return *this;
    };

    unsigned operator*() const {
      return bt_stack.back()->get_rowid();
    }

    btree * get_btree() const {
      return bt_stack.back();
    }

    bool operator!=(const BTIterator& rhs) const {
      return (bt_path != rhs.bt_path);
    };

    const bool first_bit_of_path() const {
      return bt_path.at(0);
    }
  };

  BTIterator begin() {
    return BTIterator(this);
  };

  // private:
    // BTIterator end_cache = BTIterator(this, false);

  public:
  BTIterator end() {
    // return end_cache;
    bstack stack;
    bpath path;
    stack.push_back(this);
    return BTIterator(path, stack);
  };
};

shared_ptr<btree> btreefalse(shared_ptr<btree> subtree) {
  shared_ptr<btree> out = make_shared<btree>();
  out->bfalse = subtree;
  return out;
}

shared_ptr<btree> btreetrue(shared_ptr<btree> subtree) {
  shared_ptr<btree> out = make_shared<btree>();
  out->btrue = subtree;
  return out;
}

shared_ptr<btree> btreetrue(unsigned r) {
  return btreetrue(make_shared<btree>(r));
}

shared_ptr<btree> btreefalse(unsigned r) {
  return btreefalse(make_shared<btree>(r));
}

// make a one-deep btree that is either left or right, depending on the boolean `b`.
shared_ptr<btree> btreebool(bool b, unsigned r) {
  if (b) return btreetrue(r);
  else   return btreefalse(r);
}

// // call the provided function on each leaf of the btree.
// void for_each(btree* bt, function<void(unsigned)> f) {
//   if (bt->btrue)
//     for_each(bt->btrue, f);
//   if (bt->bfalse)
//     for_each(bt->bfalse, f);
//   else if (!bt->btrue)
//     f(bt->data);
// }