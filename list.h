/*
    This file is part of Genesys.

    Genesys is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Genesys is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Genesys.  If not, see <http://www.gnu.org/licenses/>.

    Copyright (C) 2016 Clemens Kirchgatterer <clemens@1541.org>.
*/

#ifndef _LIST_H_
#define _LIST_H_

#include <assert.h>

// list template

template <class T> class ListIterator;
template <class T> class ListNode;

template <typename T>
class List {

private:

  ListNode<T> *head; // beginning of list
  ListNode<T> *tail; // end of list
  int count;         // number of nodes in list

public:

  typedef ListIterator<T> iterator;

  List(const List<T> &src);  // copy constructor
  ~List(void);               // destructor

  // default constructor
  List(void) : head(NULL), tail(NULL), count(0) {}

  // returns a reference to first element
  T &front(void) {
    assert (head != NULL);
    return head->data;
  }

  // returns a reference to last element
  T &back(void) {
    assert (tail != NULL);
    return tail->data;
  }

  // returns number of elements in list
  int size(void) {
    return count;
  }

  // returns whether or not list contains any elements
  bool empty(void) {
    return count == 0;
  }

  void clear(void) {
    while (!empty()) pop_front();
  }

  void push_front(T);    // insert element at beginning
  void push_back(T);     // insert element at end
  void pop_front(void);  // remove element from beginning
  void pop_back(void);   // remove element from end

  iterator begin() const;
  iterator end() const;
};

// Copy constructor
template <typename T>
List<T>::List(const List<T> &src) : head(NULL), tail(NULL), count(0) {
  ListNode<T> *current = src.head;

  while (current != NULL) {
    this->push_back(current->data);
    current = current->next;
  }
}

// destructor
template <typename T>
List<T>::~List(void) {
  while (!this->empty()) {
    this->pop_front();
  }
}

// insert an element at the beginning
template <typename T>
void List<T>::push_front(T d) {
  ListNode<T> *new_head = new ListNode<T>(d, head);

  if (this->empty()) {
    head = new_head;
    tail = new_head;
  } else {
    head = new_head;
  }

  count++;
}

// insert an element at the end
template <typename T>
void List<T>::push_back(T d) {
  ListNode<T> *new_tail = new ListNode<T>(d, NULL);

  if (this->empty()) {
    head = new_tail;
  } else {
    tail->next = new_tail;
  }

  tail = new_tail;
  count++;
}

// remove an element from the beginning
template <typename T>
void List<T>::pop_front(void) {
  assert(head != NULL);

  ListNode<T> *old_head = head;

  if (this->size() == 1) {
    head = NULL;
    tail = NULL;
  } else {
    head = head->next;
  }

  delete old_head;
  count--;
}

// remove an element from the end
template <typename T>
void List<T>::pop_back(void) {
  assert(tail != NULL);

  ListNode<T> *old_tail = tail;

  if (this->size() == 1) {
    head = NULL;
    tail = NULL;
  } else {
    // traverse the list to the node before tail
    ListNode<T> *current = head;

    while (current->next != tail) {
      current = current->next;
    }

    // unlink and reposition
    current->next = NULL;
    tail = current;
  }

  delete old_tail;
  count--;
}

// node implementation

template <class T> 
class ListNode {

private:

  T data;
  ListNode<T> *next;

public:

  ListNode(T d, ListNode *n = NULL) : data(d), next(n) {}

  friend class List<T>;
  friend class ListIterator<T>;
};

// iterator implementation

template <class T> class ListIterator {

public:

  typedef ListIterator<T> iterator;

  ListIterator(ListNode<T> *node) : current(node) {}
  ListIterator() : current(NULL) {}
  ListIterator(ListIterator<T> *it) : current(it.current) {}

  T &operator*();  // dereferencing operator
  iterator &operator=(const iterator &rhs);
  bool operator==(const iterator &rhs) const;
  bool operator!=(const iterator &rhs) const;
  iterator &operator++(); 
  iterator operator++(int);

protected:

   ListNode<T> *current;

   friend class List<T>;
};

template <class T>
typename List<T>::iterator List<T>::begin() const {
  return iterator(head);
}

template <class T>
typename List<T>::iterator List<T>::end() const {
  return iterator();
}

template <class T>
T &ListIterator<T>::operator*() {
  return current->data;
}

template <class T>
ListIterator<T> &ListIterator<T>::operator=(const iterator &rhs) { 
  current = rhs.current;

  return *this; 
}

template <class T>
ListIterator<T> &ListIterator<T>::operator++() {
  current = current->next;

  return *this;
}

template <class T>
ListIterator<T> ListIterator<T>::operator++(int) {
  ListIterator<T> it = *this;

  current = current->next;

  return it;
}

template<class T>
bool ListIterator<T>::operator==(const iterator &rhs) const {
  return (current == rhs.current); 
}

template <class T>
bool ListIterator<T>::operator!=(const iterator &rhs) const {
  return (current != rhs.current); 
}

#endif // _LIST_H_
