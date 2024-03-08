#include <iterator>
#include <functional>
#include <iostream>
using namespace std;

bool vecbool_neq(std::vector<bool> lhs, std::vector<bool> rhs) {
    return (lhs != rhs);
}

class btree {
public:
    unsigned rowid;
    struct btree * bfalse;
    struct btree * btrue;

    btree(unsigned d) {
        rowid = d;
    }
    btree() {}

    bool is_leaf() {
        return !(btrue || bfalse);
    };

    void print(string prefix = "") {
        if (is_leaf()) {
            cout << prefix << "Leaf: " << rowid << endl;
        } else {
            if (btrue) {
                cout << prefix << "True:" << endl;
                btrue->print(prefix + ".  ");
            }
            if (bfalse) {
                cout << prefix << "False:" << endl;
                bfalse->print(prefix + ".  ");
            }
        }
    }

    btree* force_true() {
        if (btrue) return btrue;
        btrue = new btree();
        return btrue;
    }

    btree* force_false() {
        if (bfalse) return bfalse;
        bfalse = new btree();
        return bfalse;
    }

    typedef std::vector<bool> bpath;
    typedef std::vector<btree*> bstack;

    void add_at_path(bpath path, unsigned data) {
        btree * cur_node = this;
        for (bool b : path) {
            if (b) cur_node = cur_node->force_true();
            else   cur_node = cur_node->force_false();
        }
        cur_node->rowid = data;
    };

    struct BTIterator {
        BTIterator(bpath path, bstack stack) {
            bt_path = path;
            bt_stack = stack;
        };
        
        ~BTIterator() {};

        unsigned operator*() const {
            return bt_stack.back()->rowid;
        }

        // assumes that we don't start with an empty stack
        void find_first() {
            while (true) {
                if (bt_stack.back()->btrue) {
                    bt_path.push_back(true);
                    bt_stack.push_back(bt_stack.back()->btrue);
                } else if (bt_stack.back()->bfalse) {
                    bt_path.push_back(false);
                    bt_stack.push_back(bt_stack.back()->bfalse);
                } else {
                    break;
                }
            }
        }

        BTIterator(btree* root) {
            bt_stack.push_back(root);
            find_first();
        }

        // static BTIterator begin(bpath & path, bstack & stack) {
        //     while (true) {
        //         if (stack.back()->btrue) {
        //             path.push_back(true);
        //             stack.push_back(stack.back()->btrue);
        //         } else if (stack.back()->bfalse) {
        //             path.push_back(false);
        //             stack.push_back(stack.back()->bfalse);
        //         } else {
        //             break;
        //         }
        //     }
        //     return BTIterator(path, stack);
        // };

        void dump() {
            cout << "path: ";
            for (auto b : bt_path)
                cout << b;
            cout << " stacklen: " << bt_stack.size() << " rowid: " << bt_stack.back()->rowid << endl;
        };

        BTIterator operator++() {
            if (bt_path.empty()) return *this;
            // go up while we are coming from "false" or if coming
            // from "true" but there is no "false" branch to follow.
            bt_stack.pop_back();
            while (bt_path.back() == false || !(bt_stack.back()->bfalse)) {
                if (bt_path.size() == 1 && bt_path.back() == false) {
                    // we just returned to the root, we are done.
                    bt_path.pop_back();
                    return BTIterator(bt_path, bt_stack);
                }
                bt_path.pop_back();
                bt_stack.pop_back();
            }
            bt_path.pop_back();
            bt_path.push_back(false);
            bt_stack.push_back(bt_stack.back()->bfalse);
            find_first();
            return *this;
        };

        bool operator!=(const BTIterator& rhs) {
            return (bt_path != rhs.bt_path);
        };

    private:
        bpath bt_path;
        bstack bt_stack;
    };

    BTIterator begin() {
        return BTIterator(this);
    };

    BTIterator end() {
        bpath path;
        bstack stack;
        stack.push_back(this);
        return BTIterator(path, stack);
    };
};

btree* btreetrue(btree* subtree) {
    btree * out = new btree();
    out->btrue = subtree;
    return out;
}

btree* btreefalse(btree* subtree) {
    btree * out = new btree();
    out->bfalse = subtree;
    return out;
}

btree* btreetrue(unsigned r) {
    return btreetrue(new btree(r));
}

btree* btreefalse(unsigned r) {
    return btreefalse(new btree(r));
}

// make a one-deep btree that is either left or right, depending on the boolean `b`.
btree* btreebool(bool b, unsigned r) {
    if (b) return btreetrue(r);
    else   return btreefalse(r);
}

// // call the provided function on each leaf of the btree.
// void for_each(btree* bt, function<void(unsigned)> f) {
//     if (bt->btrue)
//         for_each(bt->btrue, f);
//     if (bt->bfalse)
//         for_each(bt->bfalse, f);
//     else if (!bt->btrue)
//         f(bt->data);
// }