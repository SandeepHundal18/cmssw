//
// Top-level API for serializing and deserializing arbitrary classes
//

#ifndef GENERS_GENERICIO_HH_
#define GENERS_GENERICIO_HH_

#include "Alignment/Geners/interface/IOPointeeType.hh"
#include "Alignment/Geners/interface/binaryIO.hh"

#include "Alignment/Geners/interface/ArrayAdaptor.hh"
#include "Alignment/Geners/interface/ClearIfPointer.hh"
#include "Alignment/Geners/interface/StrippedType.hh"

namespace gs {
/**
// Generic top-level function which can be used to write out
// almost anything. Intended mainly for use inside "write"
// methods of user-developed classes and templates. Returns
// "true" if the argument item is successfully written out.
*/
template <class Stream, class Item>
inline bool write_item(Stream &os, const Item &item,
                       const bool writeClassId = true) {
  char *ps = nullptr;
  return process_const_item<GenericWriter>(item, os, ps, writeClassId);
}

/**
// A function for overwriting existing objects (which usually live
// on the stack). This function actually skips couple levels of
// indirection which would be generated by a call to "process_item".
*/
template <class Stream, class Item>
inline void restore_item(Stream &is, Item *item,
                         const bool readClassId = true) {
  typedef std::vector<ClassId> State;
  assert(item);
  State state;
  const bool status =
      GenericReader<Stream, State, Item *,
                    Int2Type<IOTraits<int>::ISPOINTER>>::process(item, is,
                                                                 &state,
                                                                 readClassId);
  if (is.fail())
    throw IOReadFailure("In gs::restore_item: input stream failure");
  if (!status)
    throw IOInvalidData("In gs::restore_item: invalid input stream data");
}

/**
// Function for returning objects on the heap. This function
// requires explicit specification of its first template
// parameter, the type of the item to read. This function
// either succeeds or throws an exception which inherits
// from std::exception.
*/
template <class Item, class Stream>
inline CPP11_auto_ptr<Item> read_item(Stream &is,
                                      const bool readClassId = true) {
  typedef std::vector<ClassId> State;
  Item *item = nullptr;
  State state;
  const bool status = GenericReader<
      Stream, State, Item *,
      Int2Type<IOTraits<int>::ISNULLPOINTER>>::process(item, is, &state,
                                                       readClassId);
  CPP11_auto_ptr<Item> ptr(item);
  if (is.fail())
    throw IOReadFailure("In gs::read_item: input stream failure");
  if (!status || item == nullptr)
    throw IOInvalidData("In gs::read_item: invalid input stream data");
  return ptr;
}

/**
// Generic top-level function for writing arrays. Note that
// the length of the array is not written out and that the
// length must be known in advance in the scope from which
// the companion function, "read_array", is called. "true"
// is returned upon success, "false" on failure.
*/
template <class Stream, class Item>
inline bool write_array(Stream &os, Item *items, const std::size_t length) {
  char *ps = nullptr;
  return process_const_item<GenericWriter>(ArrayAdaptor<Item>(items, length),
                                           os, ps, false);
}

/**
// Function for deserializing arrays. The length of the array
// must be known in the scope from which this function is invoked.
*/
template <class Stream, class Item>
inline void read_array(Stream &is, Item *items, const std::size_t length) {
  typedef std::vector<ClassId> State;
  State state;
  ArrayAdaptor<Item> adap(items, length);
  const bool st = process_item<GenericReader>(adap, is, &state, false);
  if (is.fail())
    throw IOReadFailure("In gs::read_array: input stream failure");
  if (!st)
    throw IOInvalidData("In gs::read_array: invalid input stream data");
}
} // namespace gs

namespace gs {
template <class Stream, class State, class Item, class Stage>
struct GenericWriter2 : public GenericWriter<Stream, State, Item, Stage> {};

template <class Stream, class State, class Item, class Stage>
struct GenericReader2 : public GenericReader<Stream, State, Item, Stage> {};

// The reader and writer templates should be specialized
// (that is, their "process" function should be defined) using
// the following processing stage types from "ProcessItem.hh":
//
// Int2Type<IOTraits<int>::ISPOD>                (+readIntoPtr)
// InContainerHeader
// InContainerFooter
// InContainerSize
// InPODArray
// Int2Type<IOTraits<int>::ISWRITABLE>
// Int2Type<IOTraits<int>::ISPOINTER>
// Int2Type<IOTraits<int>::ISSHAREDPTR>
// Int2Type<IOTraits<int>::ISPAIR>               (+readIntoPtr)
// Int2Type<IOTraits<int>::ISSTRING>             (+readIntoPtr)
//
// In addition, the reader should be specialized for the following
// types:
//
// InContainerCycle                              (process     only)
// Int2Type<IOTraits<int>::ISSTDCONTAINER>       (readIntoPtr only)
// Int2Type<IOTraits<int>::ISHEAPREADABLE>       (readIntoPtr only)
// Int2Type<IOTraits<int>::ISPLACEREADABLE>      (readIntoPtr only)
//
// The resulting code is essentially one big compile-time state
// machine with two main switching hubs: "process_item" function
// from "ProcessItem.hh" and "process" function in GenericReader
// template specialized for bare pointers.
//

//===================================================================
//
// Processing of a POD
//
//===================================================================
template <class Stream, class State, class T>
struct GenericWriter<Stream, State, T, Int2Type<IOTraits<int>::ISPOD>> {
  inline static bool process(const T &s, Stream &os, State *,
                             const bool processClassId) {
    static const ClassId current(ClassId::makeId<T>());
    const bool status = processClassId ? current.write(os) : true;
    if (status)
      write_pod(os, s);
    return status && !os.fail();
  }
};

template <class Stream, class State, class T>
struct GenericReader<Stream, State, T, Int2Type<IOTraits<int>::ISPOD>> {
  inline static bool readIntoPtr(T *&ptr, Stream &str, State *,
                                 const bool processClassId) {
    CPP11_auto_ptr<T> myptr;
    if (ptr == 0)
      myptr = CPP11_auto_ptr<T>(new T());
    if (processClassId) {
      static const ClassId current(ClassId::makeId<T>());
      ClassId id(str, 1);
      current.ensureSameName(id);
    }
    read_pod(str, ptr ? ptr : myptr.get());
    if (str.fail())
      return false;
    if (ptr == 0)
      ptr = myptr.release();
    return true;
  }

  inline static bool process(T &s, Stream &os, State *st,
                             const bool processClassId) {
    T *ps = &s;
    return readIntoPtr(ps, os, st, processClassId);
  }
};

//===================================================================
//
// Processing of a container header
//
//===================================================================
template <class Stream, class State, class Container>
struct GenericWriter<Stream, State, Container, InContainerHeader> {
  inline static bool process(const Container &, Stream &os, State *,
                             const bool processClassId) {
    typedef typename Container::value_type T;

    static const ClassId current(ClassId::makeId<Container>());
    bool status = processClassId ? current.write(os) : true;

    // Maybe we do not have to write out the container class id,
    // but we do have to write out the item class id -- unless the
    // container is just an array of pods. Otherwise we might not
    // be able to read the container items back.
    if (status && !(IOTraits<T>::IsPOD && IOTraits<Container>::IsContiguous)) {
      static const ClassId itemId(ClassId::makeId<T>());
      status = itemId.write(os);
    }
    return status;
  }
};

template <class Stream, class State, class Container>
struct GenericReader<Stream, State, Container, InContainerHeader> {
  inline static bool process(Container &a, Stream &is, State *state,
                             const bool processClassId) {
    typedef typename Container::value_type T;

    if (processClassId) {
      static const ClassId current(ClassId::makeId<Container>());
      ClassId id(is, 1);
      current.ensureSameName(id);
    }
    a.clear();
    if (!(IOTraits<T>::IsPOD && IOTraits<Container>::IsContiguous)) {
      ClassId id(is, 1);

      // Remember the class id of the contained items.
      // We need to do this even if the id is invalid because
      // the id will be popped back when the "InContainerFooter"
      // stage is processed.
      state->push_back(id);
    }
    return true;
  }
};

//===================================================================
//
// Processing of a container footer
//
//===================================================================
template <class Stream, class State, class Container>
struct GenericWriter<Stream, State, Container, InContainerFooter> {
  inline static bool process(const Container &, Stream &, State *, bool) {
    return true;
  }
};

template <class Stream, class State, class Container>
struct GenericReader<Stream, State, Container, InContainerFooter> {
  inline static bool process(Container &, Stream &, State *state, bool) {
    typedef typename Container::value_type T;
    if (!(IOTraits<T>::IsPOD && IOTraits<Container>::IsContiguous))
      state->pop_back();
    return true;
  }
};

//===================================================================
//
// Processing of container size
//
//===================================================================
template <class Stream, class State, class Container>
struct GenericWriter<Stream, State, Container, InContainerSize> {
  inline static bool process(const std::size_t &sz, Stream &os, State *,
                             bool /* processClassId */) {
    write_pod(os, sz);
    return !os.fail();
  }
};

template <class Stream, class State, class Container>
struct GenericReader<Stream, State, Container, InContainerSize> {
  inline static bool process(std::size_t &sz, Stream &is, State *,
                             bool /* processClassId */) {
    read_pod(is, &sz);
    return !is.fail();
  }
};

//===================================================================
//
// Processing of data in contiguous POD containers
//
//===================================================================
template <class Stream, class State, class ArrayLike>
struct GenericWriter<Stream, State, ArrayLike, InPODArray> {
  inline static bool process(const ArrayLike &a, Stream &os, State *, bool) {
    const std::size_t len = a.size();
    write_pod(os, len);
    if (len)
      write_pod_array(os, &a[0], len);
    return !os.fail();
  }
};

template <class Stream, class State, class ArrayLike>
struct GenericReader<Stream, State, ArrayLike, InPODArray> {
  inline static bool process(ArrayLike &a, Stream &s, State *, bool) {
    std::size_t len = 0;
    read_pod(s, &len);
    if (s.fail())
      return false;
    a.resize(len);
    if (!len)
      return true;
    read_pod_array(s, &a[0], len);
    return !s.fail();
  }
};

//===================================================================
//
// Processing of "writable" objects
//
//===================================================================
template <class Stream, class State, class T>
struct GenericWriter<Stream, State, T, Int2Type<IOTraits<int>::ISWRITABLE>> {
  inline static bool process(const T &s, Stream &os, State *,
                             const bool processClassId) {
    return (processClassId ? s.classId().write(os) : true) && s.write(os) &&
           !os.fail();
  }
};

template <class Stream, class State, class T>
struct GenericReader<Stream, State, T, Int2Type<IOTraits<int>::ISWRITABLE>> {
  inline static bool process(T &s, Stream &is, State *st,
                             const bool processClassId) {
    typedef IOTraits<T> M;
    T *ps = &s;
    return GenericReader<
        Stream, State, T,
        Int2Type<M::Signature &(M::ISPLACEREADABLE | M::ISHEAPREADABLE)>>::
        readIntoPtr(ps, is, st, processClassId);
  }
};

//===================================================================
//
// Processing of bare pointers.
//
// The writer simply dereferences the pointer.
//
// In the reader, we want to read stuff into the pointee object,
// or want to create an item on the heap if the pointer value is 0.
//
//===================================================================
template <class Stream, class State, class Ptr>
struct GenericWriter<Stream, State, Ptr, Int2Type<IOTraits<int>::ISPOINTER>> {
  inline static bool process(const Ptr &ptr, Stream &os, State *s,
                             const bool processClassId) {
    // Can't have pointers to pointers. This is a design
    // decision which simplifies things considerably.
    typedef typename IOPointeeType<Ptr>::type Pointee;
    typedef IOTraits<Pointee> M;
    static_assert((M::Signature & (M::ISPOINTER | M::ISSHAREDPTR)) == 0,
                  "can not write pointers to pointers");

    // Can't have NULL pointers either. But this
    // can be checked at run time only.
    assert(ptr);
    return process_const_item<GenericWriter2>(*ptr, os, s, processClassId);
  }
};

template <class Stream, class State, class Ptr>
struct GenericReader<Stream, State, Ptr, Int2Type<IOTraits<int>::ISPOINTER>> {
  inline static bool process(Ptr &ptr, Stream &str, State *s,
                             const bool processClassId) {
    // We need to figure out the type of the pointee
    // and make a swich depending on that type.
    // Note that the pointee itself can not be a pointer.
    typedef typename IOPointeeType<Ptr>::type Pointee;
    typedef IOTraits<Pointee> M;
    static_assert((M::Signature & (M::ISPOINTER | M::ISSHAREDPTR)) == 0,
                  "can not read pointers to pointers");

    return GenericReader<
        Stream, State, Pointee,
        Int2Type<M::Signature &(
            M::ISPOD | M::ISSTDCONTAINER | M::ISHEAPREADABLE |
            M::ISPLACEREADABLE | M::ISPAIR | M::ISTUPLE | M::ISEXTERNAL |
            M::ISSTRING)>>::readIntoPtr(ptr, str, s, processClassId);
  }
};

template <class Stream, class State, class Ptr>
struct GenericReader<Stream, State, Ptr,
                     Int2Type<IOTraits<int>::ISNULLPOINTER>> {
  inline static bool process(Ptr &ptr, Stream &str, State *s,
                             const bool processClassId) {
    // We need to figure out the type of the pointee
    // and make a swich depending on that type.
    // Note that the pointee itself can not be a pointer.
    typedef typename IOPointeeType<Ptr>::type Pointee;
    typedef IOTraits<Pointee> M;
    static_assert((M::Signature & (M::ISNULLPOINTER | M::ISSHAREDPTR)) == 0,
                  "can not read pointers to pointers");

    return GenericReader<
        Stream, State, Pointee,
        Int2Type<M::Signature &(
            M::ISPOD | M::ISSTDCONTAINER | M::ISPUREHEAPREADABLE |
            M::ISPLACEREADABLE | M::ISPAIR | M::ISTUPLE | M::ISEXTERNAL |
            M::ISSTRING)>>::readIntoPtr(ptr, str, s, processClassId);
  }
};

//===================================================================
//
// Processing of shared pointers -- similar logic to pointers.
// For the reader, handling of the shared pointer is reduced
// to handling of a normal pointer with 0 value.
//
//===================================================================
template <class Stream, class State, class Ptr>
struct GenericWriter<Stream, State, Ptr, Int2Type<IOTraits<int>::ISSHAREDPTR>> {
  inline static bool process(const Ptr &ptr, Stream &os, State *s,
                             const bool processClassId) {
    typedef typename Ptr::element_type Pointee;
    typedef IOTraits<Pointee> M;
    static_assert((M::Signature & (M::ISPOINTER | M::ISSHAREDPTR)) == 0,
                  "can not write pointers to pointers");
    assert(ptr.get());
    return process_const_item<GenericWriter2>(*ptr, os, s, processClassId);
  }
};

template <class Stream, class State, class ShPtr>
struct GenericReader<Stream, State, ShPtr,
                     Int2Type<IOTraits<int>::ISSHAREDPTR>> {
  inline static bool process(ShPtr &a, Stream &str, State *s,
                             const bool processClassId) {
    typedef typename ShPtr::element_type Pointee;
    typedef IOTraits<Pointee> M;
    static_assert((M::Signature & (M::ISPOINTER | M::ISSHAREDPTR)) == 0,
                  "can not read pointers to pointers");
    Pointee *ptr = 0;
    const bool status = GenericReader<
        Stream, State, Pointee *,
        Int2Type<IOTraits<int>::ISNULLPOINTER>>::process(ptr, str, s,
                                                         processClassId);
    if (status) {
      assert(ptr);
      a = CPP11_shared_ptr<Pointee>(ptr);
      return true;
    } else {
      delete ptr;
      return false;
    }
  }
};

//===================================================================
//
// Processing of std::pair
//
//===================================================================
template <class Stream, class State, class T>
struct GenericWriter<Stream, State, T, Int2Type<IOTraits<int>::ISPAIR>> {
  inline static bool process(const T &s, Stream &os, State *st,
                             const bool processClassId) {
    // Here is a little problem: in this scope "GenericWriter"
    // means GenericWriter<Stream, State, T,
    //                     Int2Type<IOTraits<int>::ISPAIR> >
    // However, we want to use the whole template, unspecialized.
    // This is why "GenericWriter2" is introduced: a copy of
    // "GenericWriter" via public inheritance.
    static const ClassId current(ClassId::makeId<T>());
    return (processClassId ? current.write(os) : true) &&
           process_const_item<GenericWriter2>(s.first, os, st, false) &&
           process_const_item<GenericWriter2>(s.second, os, st, false);
  }
};

template <class Stream, class State, class T>
struct GenericReader<Stream, State, T, Int2Type<IOTraits<int>::ISPAIR>> {
  inline static bool readIntoPtr(T *&ptr, Stream &str, State *s,
                                 const bool processClassId) {
    CPP11_auto_ptr<T> myptr;
    if (ptr == 0) {
      myptr = CPP11_auto_ptr<T>(new T());
      clearIfPointer(myptr.get()->first);
      clearIfPointer(myptr.get()->second);
    }
    std::vector<std::vector<ClassId>> itemIds;
    if (processClassId) {
      static const ClassId current(ClassId::makeId<T>());
      ClassId pairId(str, 1);
      current.ensureSameName(pairId);
      pairId.templateParameters(&itemIds);
      assert(itemIds.size() == 2U);
    } else {
      assert(!s->empty());
      s->back().templateParameters(&itemIds);
      if (itemIds.size() != 2U) {
        std::string err("In gs::GenericReader::readIntoPtr: "
                        "bad class id for std::pair on the "
                        "class id stack: ");
        err += s->back().id();
        throw IOInvalidData(err);
      }
    }
    if (!(process_item<GenericReader2>((ptr ? ptr : myptr.get())->first, str,
                                       &itemIds[0], false) &&
          process_item<GenericReader2>((ptr ? ptr : myptr.get())->second, str,
                                       &itemIds[1], false)))
      return false;
    if (ptr == 0)
      ptr = myptr.release();
    return true;
  }

  inline static bool process(T &s, Stream &os, State *st,
                             const bool processClassId) {
    T *ps = &s;
    return readIntoPtr(ps, os, st, processClassId);
  }
};

//===================================================================
//
// Processing of std::string
//
//===================================================================
template <class Stream, class State>
struct GenericWriter<Stream, State, std::string,
                     Int2Type<IOTraits<int>::ISSTRING>> {
  inline static bool process(const std::string &s, Stream &os, State *,
                             const bool processClassId) {
    static const ClassId current(ClassId::makeId<std::string>());
    const bool status = processClassId ? current.write(os) : true;
    if (status)
      write_string<char>(os, s);
    return status && !os.fail();
  }
};

template <class Stream, class State>
struct GenericReader<Stream, State, std::string,
                     Int2Type<IOTraits<int>::ISSTRING>> {
  inline static bool readIntoPtr(std::string *&ptr, Stream &is, State *,
                                 const bool processClassId) {
    CPP11_auto_ptr<std::string> myptr;
    if (ptr == nullptr)
      myptr = CPP11_auto_ptr<std::string>(new std::string());
    if (processClassId) {
      static const ClassId current(ClassId::makeId<std::string>());
      ClassId id(is, 1);
      current.ensureSameName(id);
    }
    read_string<char>(is, ptr ? ptr : myptr.get());
    if (is.fail())
      return false;
    if (ptr == nullptr)
      ptr = myptr.release();
    return true;
  }

  inline static bool process(std::string &s, Stream &is, State *st,
                             const bool processClassId) {
    std::string *ptr = &s;
    return readIntoPtr(ptr, is, st, processClassId);
  }
};

//===================================================================
//
// Processing of container readout
//
//===================================================================
template <class Stream, class State, class Container>
struct GenericReader<Stream, State, Container, InContainerCycle> {
private:
  typedef typename Container::value_type item_type;
  typedef IOTraits<item_type> M;

  // Item is a simple pointer
  inline static bool process2(Container &obj, Stream &is, State *st,
                              const std::size_t itemN, Int2Type<1>) {
    item_type ptr = 0;
    const bool status =
        GenericReader<Stream, State, item_type,
                      Int2Type<IOTraits<int>::ISNULLPOINTER>>::process(ptr, is,
                                                                       st,
                                                                       true);
    if (status) {
      assert(ptr);
      InsertContainerItem<Container>::insert(obj, ptr, itemN);
    } else
      delete ptr;
    return status;
  }

  // Item is a shared pointer
  inline static bool process2(Container &obj, Stream &is, State *st,
                              const std::size_t itemN, Int2Type<2>) {
    typedef typename item_type::element_type Pointee;
    Pointee *ptr = 0;
    const bool status =
        GenericReader<Stream, State, Pointee *,
                      Int2Type<IOTraits<int>::ISNULLPOINTER>>::process(ptr, is,
                                                                       st,
                                                                       true);
    if (status) {
      assert(ptr);
      CPP11_shared_ptr<Pointee> sptr(ptr);
      InsertContainerItem<Container>::insert(obj, sptr, itemN);
    } else
      delete ptr;
    return status;
  }

  // Item is heap-readable
  inline static bool process2(Container &obj, Stream &is, State *st,
                              const std::size_t itemN, Int2Type<3>) {
    // No class id -- this is a member of a container
    assert(!st->empty());
    item_type *ptr = item_type::read(st->back(), is);
    if (ptr) {
      InsertContainerItem<Container>::insert(obj, *ptr, itemN);
      delete ptr;
    }
    return ptr;
  }

  // Item is not a pointer and not heap-readable.
  // Assume that it has a default constructor.
  inline static bool process2(Container &obj, Stream &is, State *st,
                              const std::size_t itemN, Int2Type<4>) {
    typedef typename StrippedType<item_type>::type NCType;
    NCType item;
    NCType *pitem = &item;
    bool status =
        GenericReader<Stream, State, NCType *,
                      Int2Type<IOTraits<int>::ISPOINTER>>::process(pitem, is,
                                                                   st, false);
    if (status)
      InsertContainerItem<Container>::insert(obj, item, itemN);
    return status;
  }

public:
  inline static bool process(Container &obj, Stream &is, State *st,
                             const std::size_t itemN) {
    // By default, we will assume that container starts empty.
    // Here, we need to produce a new item. There are 3 options:
    //   1) make it on the stack, insert a copy into the container
    //   2) make it on the heap, insert a copy, delete original
    //   3) the container contains pointers to begin with, so
    //      we make it on the stack and add a pointer to the container
    return process2(
        obj, is, st, itemN,
        Int2Type<M::IsPointer * 1 + M::IsSharedPtr * 2 + M::IsHeapReadable * 3 +
                 !(M::IsPointer || M::IsSharedPtr || M::IsHeapReadable) * 4>());
  }
};

//===================================================================
//
// Reading things when a pointer is given
//
//===================================================================
template <class Stream, class State, class T>
struct GenericReader<Stream, State, T,
                     Int2Type<IOTraits<int>::ISSTDCONTAINER>> {
  inline static bool readIntoPtr(T *&ptr, Stream &str, State *s,
                                 const bool processClassId) {
    if (ptr)
      return process_item<GenericReader2>(*ptr, str, s, processClassId);
    else {
      CPP11_auto_ptr<T> myptr(new T());
      if (!process_item<GenericReader2>(*myptr, str, s, processClassId))
        return false;
      ptr = myptr.release();
      return true;
    }
  }
};

template <class Stream, class State, class T>
struct GenericReader<Stream, State, T,
                     Int2Type<IOTraits<int>::ISHEAPREADABLE>> {
  inline static bool readIntoPtr(T *&ptr, Stream &str, State *s,
                                 const bool processClassId) {
    T *readback = 0;
    if (processClassId) {
      ClassId id(str, 1);
      readback = T::read(id, str);
    } else {
      assert(!s->empty());
      readback = T::read(s->back(), str);
    }
    if (readback) {
      if (ptr) {
        try {
          // We will assume here that the "read"
          // operation takes precedence over constness
          *const_cast<typename StrippedType<T>::type *>(ptr) = *readback;
        } catch (...) {
          delete readback;
          throw;
        }
        delete readback;
      } else
        ptr = readback;
    }
    return readback;
  }
};

template <class Stream, class State, class T>
struct GenericReader<Stream, State, T,
                     Int2Type<IOTraits<int>::ISPUREHEAPREADABLE>> {
  inline static bool readIntoPtr(T *&ptr, Stream &str, State *s,
                                 const bool processClassId) {
    T *readback = nullptr;
    if (processClassId) {
      ClassId id(str, 1);
      readback = T::read(id, str);
    } else {
      assert(!s->empty());
      readback = T::read(s->back(), str);
    }
    if (readback) {
      assert(!ptr);
      ptr = readback;
    }
    return readback;
  }
};

template <class Stream, class State, class T>
struct GenericReader<Stream, State, T,
                     Int2Type<IOTraits<int>::ISPLACEREADABLE>> {
  inline static bool readIntoPtr(T *&ptr, Stream &str, State *s,
                                 const bool processClassId) {
    CPP11_auto_ptr<T> myptr;
    if (ptr == 0)
      myptr = CPP11_auto_ptr<T>(new T());
    if (processClassId) {
      ClassId id(str, 1);
      T::restore(id, str, ptr ? ptr : myptr.get());
    } else {
      assert(!s->empty());
      T::restore(s->back(), str, ptr ? ptr : myptr.get());
    }
    if (ptr == 0)
      ptr = myptr.release();
    return ptr;
  }
};
} // namespace gs

#endif // GENERS_GENERICIO_HH_
