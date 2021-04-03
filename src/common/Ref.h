/*
 * Copyright (c) 2014 - 2020 Christian Schoenebeck
 *
 * http://www.linuxsampler.org
 *
 * This file is part of LinuxSampler and released under the same terms.
 * See README file for details.
 */

#ifndef LS_REF_H
#define LS_REF_H

#include "global.h"

#include <set>
#include <stdio.h>
#if __cplusplus >= 201103L && !CONFIG_NO_CPP11STL
# include <type_traits> // for std::enable_if and std::is_same
#endif

// You may enable this while developing or at least when you encounter any kind
// of crashes or other misbehaviors in conjunction with Ref class guarded code.
// Enabling the following macro will add a bunch of sanity checks for easing
// debugging of such issues, however it comes with the cost that everything will
// be much slower.
#define LS_REF_ASSERT_MODE 0

// Enable this for VERY verbose debug messages for debugging deep issues with
// Ref class.
#define LS_REF_VERBOSE_DEBUG_MSG 0

#if LS_REF_ASSERT_MODE
# warning LS_REF_ASSERT_MODE is enabled which will decrease runtime efficiency!
# include <assert.h>
#endif

// Whether the Ref<> class shall be thread safe. Performance penalty should not
// be measurable in practice due to non blocking, lock-free atomic add, sub and
// CAS instructions being used, so this is recommended and enabled by default.
#ifndef LS_REF_ATOMIC
# define LS_REF_ATOMIC 1
#endif

//NOTE: We are using our own atomic implementation for atomic increment and
// decrement instead of std::atomic of the C++ STL, because our atomic
// implementation is lock-free and wait-free. The C++ standard just recommends,
// but does not guarantee lock-free implementation (state of the C++20 standard
// as of 2020-04-27):
// https://en.cppreference.com/w/cpp/atomic/atomic/is_lock_free
// For CAS though we simply use the STL version, since it is only used on
// resource deallocation, which is not real-time safe anyway.
#if LS_REF_ATOMIC
# include "atomic.h"
# include <atomic>
#else
# warning Ref<> class will not be thread safe (feature explicitly disabled)!
#endif

#if LS_REF_ASSERT_MODE && LS_REF_ATOMIC
# include "Mutex.h"
#endif

namespace LinuxSampler {

    template<typename T, typename T_BASE> class Ref;

    #if LS_REF_ASSERT_MODE
    extern std::set<void*> _allRefPtrs;
    #endif
    #if LS_REF_ASSERT_MODE && LS_REF_ATOMIC
    extern Mutex _allRefPtrsMutex;
    #endif

    /**
     * Exists just for implementation detail purpose, you cannot use it
     * directly. Use its derived template class Ref instead.
     *
     * @see Ref
     */
    template<typename T_BASE>
    class RefBase {
    public:
        template<typename T_BASE1>
        class _RefCounter {
        public:
            _RefCounter(T_BASE1* p, int refs, bool released) :
                #if LS_REF_ATOMIC
                references(ATOMIC_INIT(refs)),
                #else
                references(refs),
                #endif
                ptr(p)
            {
                #if LS_REF_ATOMIC
                std::atomic_store(&zombi, released);
                #endif
                #if LS_REF_VERBOSE_DEBUG_MSG
                printf("Ref %p: new counter (refs=%d)\n", ptr,
                    #if LS_REF_ATOMIC
                       atomic_read(&references)
                    #else
                       references
                    #endif
                );
                #endif
                #if LS_REF_ASSERT_MODE
                assert(p);
                assert(refs > 0);
                assert(!released);
                {
                    #if LS_REF_ATOMIC
                    LockGuard lock(_allRefPtrsMutex);
                    #endif
                    assert(!_allRefPtrs.count(p));
                    _allRefPtrs.insert(p);
                }
                #endif
            }

            virtual ~_RefCounter() {
                #if LS_REF_VERBOSE_DEBUG_MSG
                printf("Ref %p: counter destructor (refs=%d)\n", ptr,
                    #if LS_REF_ATOMIC
                       atomic_read(&references)
                    #else
                       references
                    #endif
                );
                #endif
                fflush(stdout);
            }

            void retain() {
                #if LS_REF_ATOMIC
                atomic_inc(&references);
                #else
                references++;
                #endif
                #if LS_REF_VERBOSE_DEBUG_MSG
                printf("Ref %p: retain (refs=%d)\n", ptr,
                    #if LS_REF_ATOMIC
                       atomic_read(&references)
                    #else
                       references
                    #endif
                );
                #endif
            }

            void release() {
                #if LS_REF_ATOMIC
                bool zero = atomic_dec_and_test(&references);
                # if LS_REF_VERBOSE_DEBUG_MSG
                printf("Ref %p: release (zero=%d)\n", ptr, zero);
                # endif
                if (!zero) return;
                bool expect = false;
                bool release = std::atomic_compare_exchange_strong(&zombi, &expect, true);
                if (release) deletePtr();
                #else
                if (!references) return;
                references--;
                # if LS_REF_VERBOSE_DEBUG_MSG
                printf("Ref %p: release (refs=%d)\n", ptr, references);
                # endif
                if (!references) deletePtr();
                #endif // LS_REF_ATOMIC
            }
        //protected:
            void deletePtr() {
                #if LS_REF_VERBOSE_DEBUG_MSG
                printf("RefCounter %p: deletePtr() (refs=%d)\n", ptr,
                    #if LS_REF_ATOMIC
                       atomic_read(&references)
                    #else
                       references
                    #endif
                );
                #endif
                #if LS_REF_ASSERT_MODE
                # if LS_REF_ATOMIC
                assert(!atomic_read(&references));
                # else
                assert(!references);
                # endif
                {
                    #if LS_REF_ATOMIC
                    LockGuard lock(_allRefPtrsMutex);
                    #endif
                    _allRefPtrs.erase(ptr);
                }
                #endif
                delete ptr;
                delete this;
            }

            #if LS_REF_ATOMIC
            atomic_t references;
            std::atomic<bool> zombi; ///< @c false if not released yet, @c true once @c ptr was released
            #else
            int references;
            #endif
            T_BASE1* ptr;
            //friend class ... todo
        };
        typedef _RefCounter<T_BASE> RefCounter;

        virtual ~RefBase() {
            if (refCounter) refCounter->release();
            refCounter = NULL;
        }

    //protected:
        RefCounter* refCounter;
        //friend class Ref<T_BASE, T_BASE>;

    protected:
        RefBase() : refCounter(NULL) {
            #if LS_REF_VERBOSE_DEBUG_MSG
            printf("(RefBase empty ctor)\n");
            #endif
        }
/*
        RefBase(RefCounter* rc) {
            refCounter = rc;
        }

        RefBase(const RefBase& r) {
            refCounter = r.refCounter;
            if (refCounter) refCounter->retain();
        }
*/
    };

    /**
     * Replicates a std::shared_ptr template class. Originally this class was
     * introduced to avoid a build requirement of having a C++11 compliant
     * compiler (std::shared_ptr was not part of the C++03 standard). This class
     * had been preserved though due to some advantages over the STL
     * implementation, most notably: unlike std::shared_ptr from the STL, this
     * Ref<> class provides thread safety by using a (guaranteed) lock-free and
     * wait-free implementation, which is relevant for real-time applications.
     *
     * In contrast to the STL implementation though this implementation here
     * also supports copying references of derived, different types (in a type
     * safe manner). You can achieve that by providing a second template
     * argument (which is optional), for declaring a common subtype. For example
     * the following code would not compile:
     * @code
     * void example(UILabel* pLabel) {
     *     Ref<UILabel> lbl = pLabel;
     *     Ref<UIWidget> w = lbl; // compile error, incompatible Ref types
     *     w->resize(16,300);
     * }
     * @endcode
     * Whereas the following would work:
     * @code
     * void example(UILabel* pLabel) {
     *     Ref<UILabel,UIWidget> lbl = pLabel;
     *     Ref<UIWidget> w = lbl; // works (assuming that UILabel is a subclass of UIWidget)
     *     w->resize(16,300);
     * }
     * @endcode
     * Like the STL's std::shared_ptr, this class also emulates raw pointer
     * access and operators. With one addition: if used in the derived common
     * subtype manner as shown above, access to the actual data and boolean
     * operator will also check whether the underlying pointer (of the common
     * subclass) can actually be casted safely to the objects main type (first
     * template argument of this class). For example:
     * @code
     * void example(UILabel* pLabel) { // assuming pLabel is not NULL ...
     *     Ref<UILabel,UIWidget> lbl = pLabel;
     *     Ref<UIDialog,UIWidget> dlg = lbl;
     *     bool b1 = lbl; // will be true (assuming pLabel was not NULL)
     *     bool b2 = dlg; // will be false (assuming that UIDialog is not derived from UILabel)
     *     lbl->setText("foo"); // works
     *     dlg->showModal(); // would crash with -> operator providing a NULL pointer
     * }
     * @endcode
     * Like with std::shared_ptr you must be @b very cautious that you
     * initialize only one Ref class object directly with the same raw pointer.
     * If you forget this fundamental rule somewhere, your code will crash!
     * @code
     * UIWidget* ptr = new UIWidget();
     * Ref<UIWidget> w1 = ptr;
     * Ref<UIWidget> w2 = w1;  // this is OK, copy from a Ref object
     * Ref<UIWidget> w3 = ptr; // illegal! 2nd direct init from same raw pointer. This will crash!
     * @endcode
     * It would be possible to write an implementation of the Ref class that
     * could handle the case above as well without crashing, however it would be
     * too slow for practice. Because it would require a global lookup table
     * maintaining all memory pointers which are currently already guarded by
     * this class. Plus it would need an expensive synchronization to prevent
     * concurrent access on that global lookup table.
     */
    template<typename T, typename T_BASE = T>
    class Ref : public RefBase<T_BASE> {
    public:
        typedef RefBase<T_BASE> RefBaseT;
        typedef typename RefBase<T_BASE>::RefCounter RefCounter;

        Ref() : RefBaseT() {
            #if LS_REF_VERBOSE_DEBUG_MSG
            printf("Ref empty ctor Ref:%p\n", this);
            #endif
        }

        #if __cplusplus >= 201103L && !CONFIG_NO_CPP11STL
        template<std::enable_if<!std::is_same<T_BASE, T>::value>* = NULL> // prevent compiler error if T == T_Base (due to method signature duplicate)
        #endif
        Ref(const T_BASE* p) : RefBaseT() {
            #if LS_REF_VERBOSE_DEBUG_MSG
            printf("Ref base ptr ctor Ref:%p <- p:%p\n", this, p);
            #endif
            RefBaseT::refCounter = p ? new RefCounter((T_BASE*)p, 1, false) : NULL;
        }

        Ref(const T* p) : RefBaseT() {
            #if LS_REF_VERBOSE_DEBUG_MSG
            printf("Ref main ptr ctor Ref:%p <- p:%p\n", this, p);
            #endif
            RefBaseT::refCounter = p ? new RefCounter((T*)p, 1, false) : NULL;
        }

        Ref(const RefBaseT& r) : RefBaseT() {
            #if LS_REF_VERBOSE_DEBUG_MSG
            printf("Ref base ref ctor Ref:%p <- Ref:%p\n", this, &r);
            #endif
            RefBaseT::refCounter = r.refCounter;
            if (RefBaseT::refCounter)
                RefBaseT::refCounter->retain();
        }

        Ref(const Ref& r) : RefBaseT() {
            #if LS_REF_VERBOSE_DEBUG_MSG
            printf("Ref main ref ctor Ref:%p <- Ref:%p\n", this, &r);
            #endif
            RefBaseT::refCounter = r.refCounter;
            if (RefBaseT::refCounter)
                RefBaseT::refCounter->retain();
        }

        inline T* operator->() {
            return dynamic_cast<T*>( RefBaseT::refCounter->ptr );
        }

        inline const T* operator->() const {
            return dynamic_cast<const T*>( RefBaseT::refCounter->ptr );
        }

        inline T& operator*() {
            return *dynamic_cast<T*>( RefBaseT::refCounter->ptr );
        }

        inline const T& operator*() const {
            return *dynamic_cast<const T*>( RefBaseT::refCounter->ptr );
        }

        inline bool operator==(const RefBaseT& other) const {
            return RefBaseT::refCounter == other.refCounter;
        }

        inline bool operator!=(const RefBaseT& other) const {
            return RefBaseT::refCounter != other.refCounter;
        }

        inline operator bool() const {
            return RefBaseT::refCounter && RefBaseT::refCounter->ptr &&
                   dynamic_cast<const T*>( RefBaseT::refCounter->ptr );
        }

        inline bool operator!() const {
            return !( RefBaseT::refCounter && RefBaseT::refCounter->ptr &&
                      dynamic_cast<const T*>( RefBaseT::refCounter->ptr ) );
        }

/*
        inline operator RefBaseT&() {
            return *this;
        }

        inline operator const RefBaseT&() const {
            return *this;
        }
*/
        inline bool isEquivalent(const RefBaseT& other) const {
            if (static_cast<const RefBaseT*>(this) == &other)
                return true;
            return (RefBaseT::refCounter == other.refCounter);
        }

        inline bool isEquivalent(const T_BASE* const other) const {
            if (!other) return !RefBaseT::refCounter;
            if (!RefBaseT::refCounter) return false;
            return other == RefBaseT::refCounter->ptr;
        }

        Ref<T,T_BASE>& operator=(const RefBaseT& other) {
            #if LS_REF_VERBOSE_DEBUG_MSG
            printf("Ref base ref assignment Ref:%p <- Ref:%p\n", this, &other);
            #endif
            if (isEquivalent(other)) {
                #if LS_REF_VERBOSE_DEBUG_MSG
                printf("Ref %p WRN: equivalent ref assignment ignored.\n", this);
                #endif
                return *this;
            }
            if (RefBaseT::refCounter) {
                RefBaseT::refCounter->release();
                RefBaseT::refCounter = NULL;
            }
            RefBaseT::refCounter = other.refCounter;
            if (RefBaseT::refCounter)
                RefBaseT::refCounter->retain();
            return *this;
        }

        Ref<T,T_BASE>& operator=(const Ref<T,T_BASE>& other) {
            #if LS_REF_VERBOSE_DEBUG_MSG
            printf("Ref main ref assignment Ref:%p <- Ref:%p\n", this, &other);
            #endif
            if (isEquivalent(other)) {
                #if LS_REF_VERBOSE_DEBUG_MSG
                printf("Ref %p WRN: equivalent ref assignment ignored.\n", this);
                #endif
                return *this;
            }
            if (RefBaseT::refCounter) {
                RefBaseT::refCounter->release();
                RefBaseT::refCounter = NULL;
            }
            RefBaseT::refCounter = other.refCounter;
            if (RefBaseT::refCounter)
                RefBaseT::refCounter->retain();
            return *this;
        }

        Ref<T,T_BASE>& operator=(const T* p) {
            #if LS_REF_VERBOSE_DEBUG_MSG
            printf("Ref main ptr assignment Ref:%p <- p:%p\n", this, p);
            #endif
            if (isEquivalent(p)) {
                #if LS_REF_VERBOSE_DEBUG_MSG
                printf("Ref %p WRN: equivalent ptr assignment ignored.\n", this);
                #endif
                return *this;
            }
            if (RefBaseT::refCounter) {
                RefBaseT::refCounter->release();
                RefBaseT::refCounter = NULL;
            }
            RefBaseT::refCounter = p ? new RefCounter((T*)p, 1, false) : NULL;
            #if LS_REF_VERBOSE_DEBUG_MSG
            printf("Ref main ptr assignment done\n");
            #endif
            return *this;
        }

        #if __cplusplus >= 201103L && !CONFIG_NO_CPP11STL
        template<std::enable_if<!std::is_same<T_BASE, T>::value>* = NULL> // prevent compiler error if T == T_Base (due to method signature duplicate)
        #endif
        Ref<T,T_BASE>& operator=(const T_BASE* p) {
            #if LS_REF_VERBOSE_DEBUG_MSG
            printf("Ref base ptr assignment Ref:%p <- p:%p\n", this, p);
            #endif
            if (isEquivalent(p)) {
                #if LS_REF_VERBOSE_DEBUG_MSG
                printf("Ref %p WRN: equivalent ptr assignment ignored.\n", this);
                #endif
                return *this;
            }
            if (RefBaseT::refCounter) {
                RefBaseT::refCounter->release();
                RefBaseT::refCounter = NULL;
            }
            RefBaseT::refCounter = p ? new RefCounter((T*)p, 1, false) : NULL;
            return *this;
        }
    };

} // namespace LinuxSampler

#endif // LS_REF_H
