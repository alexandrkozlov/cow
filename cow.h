#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include <algorithm>

namespace cow
{
    /**
     * This is copy on write vector implmentation with short synchronizations.
     * Read operations take a copy with short blocking just to get a copy.
     * Write operations are synchronized and it makes a copy of data if somebody keeps readonly copy.
     *
     * @author Alexander Kozlov
     */
    template <typename T, typename TLock = std::mutex, typename TLocker = std::lock_guard<TLock>, typename TAlloc = std::allocator<T>>
    class vector
    {
    private:
        typedef vector<T, TLock, TLocker> TVector;
        typedef std::vector<T, TAlloc> TStorage;
        typedef std::shared_ptr<TStorage> TStoragePtr;

    public:
        vector()
        {
        }

        vector(const TLock& lock)
            : _lock(lock)
        {
        }

        vector(const TVector& array)
            : _storage(array.copy())
        {
        }

        void clear()
        {
            TLocker locker(_lock);
            _storage.reset();
        }

        TVector& operator=(const TVector& _Right)
        {
            TStoragePtr storage_copy = _Right.copy();

            {
                TLocker locker(_lock);
                _storage = storage_copy;
            }

            return *this;
        }

        void push_front(const T& t)
        {
            TLocker locker(_lock);

            if (_storage.use_count() == 1) // nobody holds read-only copy of vector
                _storage->insert(_storage->begin(), t);
            else // somebody has a read-only copy
            {
                TStoragePtr newStorage = TStoragePtr(new TStorage());

                if (!_storage)
                {
                    newStorage->reserve(4);
                    newStorage->push_back(t);
                }
                else // copy everything
                {
                    newStorage->reserve(_storage->size() + 1 + 4/*reserve additional elements*/);
                    newStorage->push_back(t);
                    newStorage->insert(newStorage->end(), _storage->begin(), _storage->end());
                }
                _storage = newStorage;
            }
        }

        void push_back(const T& t)
        {
            TLocker locker(_lock);

            if (_storage.use_count() == 1) // nobody holds read-only copy of vector
                _storage->push_back(t);
            else // somebody has a read-only copy
            {
                TStoragePtr newStorage = TStoragePtr(new TStorage());
                if (!_storage)
                    newStorage->reserve(4);
                else // copy everything
                {
                    newStorage->reserve(_storage->size() + 1 + 4/*reserve additional elements*/);
                    newStorage->insert(newStorage->end(), _storage->begin(), _storage->end());
                }
                newStorage->push_back(t);
                _storage = newStorage;
            }
        }

        template< class... Args>
        void emplace_back(Args&&... args)
        {
            TLocker locker(_lock);

            if (_storage.use_count() == 1) // nobody holds read-only copy of vector
                _storage->emplace_back(_STD forward<Args>(args)...);
            else // somebody has a read-only copy
            {
                TStoragePtr newStorage = TStoragePtr(new TStorage());
                if (!_storage)
                    newStorage->reserve(4);
                else // copy everything
                {
                    newStorage->reserve(_storage->size() + 1 + 4/*reserve additional elements*/);
                    newStorage->insert(newStorage->end(), _storage->begin(), _storage->end());
                }
                newStorage->emplace_back(_STD forward<Args>(args)...);
                _storage = newStorage;
            }
        }

        template <typename _Pred>
        std::size_t remove(_Pred predicate)
        {
            TLocker locker(_lock);

            if (!_storage || _storage->empty())
                return 0;

            std::size_t count = 0;

            if (_storage.use_count() == 1) // nobody holds read-only copy of vector
            {
                for (auto it = _storage->begin(); it != _storage->end(); )
                {
                    if (!predicate(*it))
                        ++it;
                    else
                    {
                        it = _storage->erase(it);
                        ++count;
                    }
                }
            }
            else // somebody has a read-only copy
            {
                TStoragePtr newStorage = TStoragePtr(new TStorage());
                newStorage->reserve(_storage->size());

                for (auto const& elem : *_storage)
                {
                    if (predicate(elem))
                        ++count;
                    else
                        newStorage->push_back(elem);
                }

                if (_storage->size() == newStorage->size()) // otherwise nothing changed
                    return count;

                if (newStorage->empty())
                    _storage.reset();
                else
                    _storage = newStorage;
            }

            return count;
        }

        template <typename _Pred>
        bool removeFirst(_Pred predicate)
        {
            TLocker locker(_lock);

            if (!_storage || _storage->empty())
                return false;

            auto it = std::find_if(_storage->begin(), _storage->end(), predicate);
            
            return removeAt(it);
        }

        template <typename _Pred>
        bool removeLast(_Pred predicate)
        {
            TLocker locker(_lock);

            if (!_storage || _storage->empty())
                return false;

            auto rit = std::find_if(_storage->rbegin(), _storage->rend(), predicate);
            if (rit == _storage->rend())
                return false;

            return removeAt(--rit.base());
        }

        template <typename _Pred>
        bool exists(_Pred predicate) const
        {
            TStoragePtr storage_copy = copy();

            if (!storage_copy || _storage->empty())
                return false;

            for (auto const& elem : *storage_copy)
                if (predicate(elem))
                    return true;

            return false;
        }

        template <typename _Pred, typename _DefaultValue>
        T find_first(_Pred predicate, _DefaultValue default_value) const
        {
            TStoragePtr storage_copy = copy();

            if (!storage_copy || _storage->empty())
                return default_value;

            for (auto const& elem : *storage_copy)
                if (predicate(elem))
                    return elem;

            return default_value;
        }

        template <typename _Pred, typename _DefaultValue>
        T find_last(_Pred predicate, _DefaultValue default_value) const
        {
            TStoragePtr storage_copy = copy();

            if (!storage_copy || _storage->empty())
                return default_value;

            for (auto it = storage_copy->rbegin(); it != storage_copy->rend(); ++it)
                if (predicate(*it))
                    return *it;

            return default_value;
        }

        class iterator
        {
        public:
            iterator() = default; // constructor for end iterator
            iterator(const iterator & copy) = default;

            iterator(const TStoragePtr & storage, typename TStorage::const_iterator const& begin, typename TStorage::const_iterator const& end)
                : _storage(storage)
                , _it(begin)
                , _end(end)
            {
            }

            iterator& operator=(iterator const & right) = default;

            typename TStorage::const_reference operator*() const
            {
                return _it.operator*();
            }

            typename TStorage::pointer operator->() const
            {
                return _it.operator->();
            }

            iterator& operator++()
            {
                ++_it;
                return *this;
            }

            iterator operator++(int)
            {
                _it++;
                return *this;
            }

            iterator& operator--()
            {
                --_it;
                return *this;
            }

            iterator operator--(int)
            {
                _it--;
                return *this;
            }

            bool operator==(iterator const & right) const
            {
                if (!_storage) // this is end() iterator
                {
                    if (!right._storage) // right value is also end()
                        return true;

                    return right._end == right._it;
                }
                else // this is usual iterator
                {
                    if (!right._storage) // right value is end()
                        return _it == _end;

                    return _it == right._it;
                }
            }

            bool operator!=(iterator const & right) const
            {	// test for iterator inequality
                return (!(*this == right));
            }

        private:
            TStoragePtr _storage;

            typename TStorage::const_iterator _it;
            typename TStorage::const_iterator _end;
        };

        iterator begin() const
        {
            TLocker locker(_lock);
            return _storage ? iterator(_storage, _storage->begin(), _storage->end()) : iterator();
        }

        iterator end() const
        {
            return iterator();
        }

        // Read-only copy for access elements by index and etc.
        class readonly_vector
        {
        public:
            readonly_vector() = delete;

            readonly_vector(const TStoragePtr & storage)
            {
                assign(storage);
            }

            readonly_vector(const readonly_vector & copy)
            {
                assign(copy._storage);
            }

            readonly_vector& operator=(const readonly_vector& _Right)
            {
                assign(_Right._storage);

                return *this;
            }

            bool empty() const
            {
                return _storage->empty();
            }

            typename TStorage::size_type size() const
            {
                return _storage->size();
            }

            typename TStorage::const_reference at(typename TStorage::size_type pos) const
            {
                return _storage->at(pos);
            }

            typename TStorage::const_reference operator[](typename TStorage::size_type pos) const
            {
                return _storage->operator[](pos);
            }

            typename TStorage::const_reference front() const
            {
                return _storage->front();
            }

            typename TStorage::const_reference back() const
            {
                return _storage->back();
            }

            const T* data() const
            {
                return _storage->data();
            }

            typename TStorage::const_iterator begin() const { return _storage->begin(); }
            typename TStorage::const_iterator cbegin() const { return _storage->cbegin(); }
            typename TStorage::const_iterator end() const { return _storage->end(); }
            typename TStorage::const_iterator cend() const { return _storage->cend(); }

            typename TStorage::const_iterator rbegin() const { return _storage->rbegin(); }
            typename TStorage::const_iterator crbegin() const { return _storage->crbegin(); }
            typename TStorage::const_iterator rend() const { return _storage->rend(); }
            typename TStorage::const_iterator crend() const { return _storage->crend(); }

            operator TStorage const& () const { return *_storage; }

        private:
            void assign(const TStoragePtr & storage)
            {
                _storage = !storage ? _empty_storage : storage;
            }

            TStoragePtr _storage;

            static TStoragePtr _empty_storage;
        };

        readonly_vector read_only_copy() const
        {
            TLocker locker(_lock);
            return readonly_vector(_storage);
        }

        TLock& lock() const
        {
            return _lock;
        }

        // This method should be called only under lock
        TStorage& data()
        {
            if (!_storage) // we need to create empty array for direct access
                _storage = TStoragePtr(new TStorage());

            return *_storage;
        }

    private:
        TStoragePtr copy() const
        {
            TLocker locker(_lock);
            return _storage;
        }

        bool removeAt(typename TStorage::iterator it)
        {
            if (it == _storage->end())
                return false;

            if (_storage->size() == 1) // it's single element and will remove it
                _storage.reset();
            else if (_storage.use_count() == 1) // nobody holds read-only copy of vector
                _storage->erase(it);
            else // somebody has a read-only copy
            {
                TStoragePtr newStorage = TStoragePtr(new TStorage());
                newStorage->reserve(_storage->size());

                if (it != _storage->begin())
                    newStorage->insert(newStorage->end(), _storage->begin(), it);
                if (++it != _storage->end())
                    newStorage->insert(newStorage->end(), it, _storage->end());

                _storage = newStorage;
            }

            return true;
        }

    private:
        TStoragePtr _storage;
        mutable TLock _lock;
    };

    template <typename T, typename TLock, typename TLocker, typename TAlloc>
    typename vector<T, TLock, TLocker, TAlloc>::TStoragePtr vector<T, TLock, TLocker, TAlloc>::readonly_vector::_empty_storage = std::make_shared< vector<T, TLock, TLocker, TAlloc>::TStorage >();
}
