/* $Id: MenuBackend.h 1231 2011-08-25 10:57:49Z abrevig $
||
|| @author         Alexander Brevig <abrevig@wiring.org.co>
|| @url            http://wiring.org.co/
|| @contribution   Adrian Brzezinski
|| @contribution   Bernhard Benum
|| @contribution   Brett Hagman <bhagman@wiring.org.co>
|| @contribution   Ryan Michael <kerinin@gmail.com>
||
|| @description
|| | Provides an easy way of making menus.
|| |
|| | Wiring Cross-platform Library
|| #
||
|| @license Please see cores/Common/License.txt.
||
*/

#ifndef MENUBACKEND_H
#define MENUBACKEND_H
#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
#endif

class MenuBackend; //forward declaration of the menu backend
class MenuItem; //forward declaration of the menu item

struct MenuChangeEvent
{
  //const MenuItem &from;
  const MenuItem &to;
};

struct MenuUseEvent
{
  MenuItem &item;
};

struct MenuItemChangeEvent
{
  const MenuItem &item;
};

struct MenuMoveEvent
{
  const MenuItem &item;
};

typedef void (*cb_change)(MenuChangeEvent);
typedef void (*cb_use)(MenuUseEvent);
typedef void (*cb_move)(MenuMoveEvent);


/*
  A menu item will be a container for an item that is a part of a menu
  Each such item has a logical position in the hierarchy as well as a text and maybe a mnemonic shortkey
*/
class MenuItem
{
  public:    
    const __FlashStringHelper *name;    

    /*
    || @constructor
    || | Basic item with a name and an optional mnemonic  [File (f)]
    || #
    ||
    || @example
    || | MenuItem file = MenuItem("File",'F');
    || #
    ||
    || @parameter itemName  the name of the item
    || @parameter shortKey the mnemonic 'shortkey'
    */
    MenuItem(const __FlashStringHelper *itemName, uint8_t shortKey = 0) : name(itemName), shortkey(shortKey)
    {
      before = right = after = left = 0;
      menuBackend = 0;
    }

    /*
    || @description
    || | Register a backend for this item to use for callbacks and such
    || #
    ||
    || @parameter mb the menu backend that controls this item
    */
    inline void registerBackend(MenuBackend &mb)
    {
      menuBackend = &mb;
    }

    //void use(){} //update some internal data / statistics
    /*
    || @description
    || | Get the name of this item
    || #
    ||
    || @return The name of this item
    */
    inline const __FlashStringHelper * getName() const
    {
      return name;
    }
    /*
    || @description
    || | Get the shortkey of this item
    || #
    ||
    || @return The shortkey of this item
    */
    inline uint8_t getShortkey() const
    {
      return shortkey;
    }
    /*
    || @description
    || | Check to see if this item has a shorkey
    || #
    ||
    || @return true if this item has a shorkey
    */
    inline const bool hasShortkey() const
    {
      return (shortkey != '\0');
    }

    /*
    || @description
    || | Check to see if this item has children
    || #
    ||
    || @return true if the item has children
    */
    inline const bool hasChildren() const
    {
      return (before || right || after || left);
    }

    /*
    || @description
    || | Get the item 'before' this item
    || #
    ||
    || @return the item before this
    */
    inline MenuItem *getBefore() const
    {
      return before;
    }
    /*
    || @description
    || | Get the item 'right' of this item
    || #
    ||
    || @return the item right of this
    */
    inline MenuItem *getRight() const
    {
      return right;
    }
    /*
    || @description
    || | Get the item 'after' this item
    || #
    ||
    || @return the item after this
    */
    inline MenuItem *getAfter() const
    {
      return after;
    }
    /*
    || @description
    || | Get the item 'left' of this item
    || #
    ||
    || @return the item left of this
    */
    inline MenuItem *getLeft() const
    {
      return left;
    }

    //default vertical menu
    /*
    || @description
    || | Add an item after this item in the hierarchy
    || #
    ||
    || @parameter mi the item to add
    || @return the item sent as parameter for chaining
    */
    MenuItem &add(MenuItem &mi)
    {
      return addAfter(mi);
    }

    /*
    || @description
    || | Add an item before this item in the hierarchy
    || #
    ||
    || @parameter mi the item to add
    || @return the item sent as parameter for chaining
    */
    MenuItem &addBefore(MenuItem &mi)
    {
      mi.after = this;
      before = &mi;
      return mi;
    }
    /*
    || @description
    || | Add an item right to this item in the hierarchy
    || #
    ||
    || @parameter mi the item to add
    || @return the item sent as parameter for chaining
    */
    MenuItem &addRight(MenuItem &mi)
    {
      mi.left = this;
      right = &mi;
      return mi;
    }
    /*
    || @description
    || | Add an item after this item in the hierarchy
    || #
    ||
    || @parameter mi the item to add
    || @return the item sent as parameter for chaining
    */
    MenuItem &addAfter(MenuItem &mi)
    {
      mi.before = this;
      after = &mi;
      return mi;
    }
    /*
    || @description
    || | Add an item left of this item in the hierarchy
    || #
    ||
    || @parameter mi the item to add
    || @return the item sent as parameter for chaining
    */
    MenuItem &addLeft(MenuItem &mi)
    {
      mi.right = this;
      left = &mi;
      return mi;
    }
    /*
    || @description
    || | Set a callback to be fired before any 'move' function is called with this item 
    || | as the current MenuItem.  
    || #
    || 
    || @paramter cb The callback to be fired
    || @return this MenuItem
    */
   /*
    MenuItem &onChangeFrom(cb_change cb)
    {
      cb_onChangeFrom = cb;
      return *this;
    }*/
     /*
    || @description
    || | Set a callback to be fired after any 'move' function is called with this item 
    || | as the resulting MenuItem.  
    || #
    || 
    || @paramter cb The callback to be fired
    || @return this MenuItem
    */
   /*
    MenuItem &onChangeTo(cb_change cb)
    {
      cb_onChangeTo = cb;
      return *this;
    }
 */
   /*
    || @description
    || | Set a callback to be fired when 'moveUp' is called with this item as the current MenuItem
    || #
    || 
    || @paramter cb The callback to be fired
    || @return this MenuItem
    */
   /*
    MenuItem &onUp(cb_move cb)
    {
      cb_onUp = cb;
      return *this;
    }*/
    /*
    || @description
    || | Set a callback to be fired when 'moveDown' is called with this item as the current MenuItem
    || #
    || 
    || @paramter cb The callback to be fired
    || @return this MenuItem
    */
   /*
    MenuItem &onDown(cb_move cb)
    {
      cb_onDown = cb;
      return *this;
    }*/
    /*
    || @description
    || | Set a callback to be fired when 'moveLeft' is called with this item as the current MenuItem
    || #
    || 
    || @paramter cb The callback to be fired
    || @return this MenuItem
    */
   /*
    MenuItem &onLeft(cb_move cb)
    {
      cb_onLeft = cb;
      return *this;
    }*/
    /*
    || @description
    || | Set a callback to be fired when 'moveRight' is called with this item as the current MenuItem
    || #
    || 
    || @paramter cb The callback to be fired
    || @return this MenuItem
    */
   /*
    MenuItem &onRight(cb_move cb)
    {
      cb_onRight = cb;
      return *this;
    }*/
    /*
    || @description
    || | Set a callback to be fired when 'use' is called with this item as the current MenuItem
    || #
    || 
    || @paramter cb The callback to be fired
    || @return this MenuItem
    */
   /*
    MenuItem &onUse(cb_use cb)
    {
      cb_onUse = cb;
      return *this;
    }*/

  protected:
    uint8_t shortkey;
    
    MenuItem *before;
    MenuItem *right;
    MenuItem *after;
    MenuItem *left;

    //cb_change cb_onChangeFrom;
    // cb_change cb_onChangeTo;
    // cb_move cb_onUp;
    // cb_move cb_onDown;
    // cb_move cb_onLeft;
    // cb_move cb_onRight;
    // cb_use cb_onUse;

    MenuBackend *menuBackend;

  private:
    friend class MenuBackend;

    MenuItem *moveUp()
    {
      return before;
    }

    MenuItem *moveDown()
    {
      return after;
    }

    MenuItem *moveLeft()
    {
      return left;
    }

    MenuItem *moveRight()
    {
      return right;
    }

};

class MenuBackend
{
  public:
    MenuBackend(cb_use menuUse, cb_change menuChange = 0) : root(F(""))
    {
      current = &root;
      cb_menuChange = menuChange;
      cb_menuUse = menuUse;
    }

    /*
    || @description
    || | Get the root of this menu
    || #
    ||
    || @return the root item
    */
    MenuItem &getRoot()
    {
      return root;
    }
    /*
    || @description
    || | Get the current item of this menu
    || #
    ||
    || @return the current item
    */
    MenuItem &getCurrent()
    {
      return *current;
    }

    /*
    || @description
    || | Move up in the menu structure, will fire move event
    || #
    */
    void moveUp()
    {/*
      if (current->cb_onUp)
      {
        MenuMoveEvent mme = { *current };
        (*current->cb_onUp)(mme);
      }*/
      setCurrent(current->moveUp());
    }
    /*
    || @description
    || | Move down in the menu structure, will fire move event
    || #
    */
    void moveDown()
    {/*
      if (current->cb_onDown)
      {
        MenuMoveEvent mme = { *current };
        (*current->cb_onDown)(mme);
      }*/
      setCurrent(current->moveDown());
    }
    /*
    || @description
    || | Move left in the menu structure, will fire move event
    || #
    */
    void moveLeft()
    {
    /*
      if (current->cb_onLeft)
      {
        MenuMoveEvent mme = { *current };
        (*current->cb_onLeft)(mme);
      }*/
      setCurrent(current->moveLeft());
    }
    /*
    || @description
    || | Move right in the menu structure, will fire move event
    || #
    */
    void moveRight()
    {/*
      if (current->cb_onRight)
      {
        MenuMoveEvent mme = { *current };
        (*current->cb_onRight)(mme);
      }*/
      setCurrent(current->moveRight());
    }
    /*
    || @description
    || | Use an item
    || #
    ||
    || @parameter item is the item to use
    */
    void use(MenuItem &item)
    {
      setCurrent(&item);
      use();
    }
    /*
    || @description
    || | Use an item per its shortkey
    || #
    ||
    || @parameter shortkey the shortkey of the target item
    */
    void use(uint8_t shortkey)
    {
      recursiveSearch(shortkey, &root);
      use();
    }
    /*
    || @description
    || | Use an item, will fire use event
    || #
    */
    void use()
    {
      //current->use();
      /*
      if (current->cb_onUse)
      {
        MenuUseEvent mue = { *current };
        (*current->cb_onUse)(mue);
      }*/
      if (cb_menuUse)
      {
        MenuUseEvent mue = { *current };
        cb_menuUse(mue);
      }
    }
    /*
    || @description
    || | Select an item, will fire change event
    || #
    */
    void select(MenuItem &item)
    {
      setCurrent(&item);
    }

  private:
    void setCurrent(MenuItem *next)
    {
      if (next)
      {
        MenuChangeEvent mce = { *next };
        /*
        if (current->cb_onChangeFrom)
        {
          (*current->cb_onChangeFrom)(mce);
        }*/
          /*
        if (next->cb_onChangeTo)
        {
          (*next->cb_onChangeTo)(mce);
        }*/
        if (cb_menuChange)
        {
          (*cb_menuChange)(mce);
        }
        current = next;
      }
    }

    void foundShortkeyItem(MenuItem *mi)
    {
      current = mi;
    }

    char canSearch(uint8_t shortkey, MenuItem *m)
    {
      if (m == 0)
      {
        return 0;
      }
      else
      {
        if (m->getShortkey() == shortkey)
        {
          foundShortkeyItem(m);
          return 1;
        }
        return -1;
      }
    }

    void rSAfter(uint8_t shortkey, MenuItem *m)
    {
      if (canSearch(shortkey, m) != 1)
      {
        rSAfter(shortkey, m->getAfter());
        rSRight(shortkey, m->getRight());
        rSLeft(shortkey, m->getLeft());
      }
    }

    void rSRight(uint8_t shortkey, MenuItem *m)
    {
      if (canSearch(shortkey, m) != 1)
      {
        rSAfter(shortkey, m->getAfter());
        rSRight(shortkey, m->getRight());
        rSBefore(shortkey, m->getBefore());
      }
    }

    void rSLeft(uint8_t shortkey, MenuItem *m)
    {
      if (canSearch(shortkey, m) != 1)
      {
        rSAfter(shortkey, m->getAfter());
        rSLeft(shortkey, m->getLeft());
        rSBefore(shortkey, m->getBefore());
      }
    }

    void rSBefore(uint8_t shortkey, MenuItem *m)
    {
      if (canSearch(shortkey, m) != 1)
      {
        rSRight(shortkey, m->getRight());
        rSLeft(shortkey, m->getLeft());
        rSBefore(shortkey, m->getBefore());
      }
    }

    void recursiveSearch(uint8_t shortkey, MenuItem *m)
    {
      if (canSearch(shortkey, m) != 1)
      {
        rSAfter(shortkey, m->getAfter());
        rSRight(shortkey, m->getRight());
        rSLeft(shortkey, m->getLeft());
        rSBefore(shortkey, m->getBefore());
      }
    }

    MenuItem root;
    MenuItem *current;

    cb_change cb_menuChange;
    cb_use cb_menuUse;
};

#endif
// MENUBACKEND_H
