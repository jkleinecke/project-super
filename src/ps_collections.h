
// define TEST_COLLECTIONS to include the testing routines
// #define TEST_COLLECTIONS 1

// NOTE(james): intended to be used as a POD container.  Not suitable as a replacement for std::vector<>!!
// Does not automatically grow!
// Does not perform runtime bounds checking or throw, will only ASSERT! YOU still have to not overrun the end of the array!
// insert() moves the element at the index to the end of the array to make room rather than moving all the elements down a position
// erase() moves the element at the end of the array to the slot being erased


template<typename T>
struct slice
{
    T* _data;
    u32 _size;

    slice slice_leftof(u32 last)
    {
        return sub_slice(0, last);
    }

    slice sub_slice(u32 first, u32 size)
    {
        ASSERT((first + size) <= _size); 
        return slice { ._data = _data + first, ._size = size }; 
    }

    slice slice_rightof(u32 first)
    {
        return sub_slice(first, _size - first);
    }

    T* data() { return _data; }
    const T* data() const { return _data; }

    u32 size() const { return _size; }
    b32 empty() const { return _size == 0; }

    T& front() const { ASSERT(_size > 0); return _data[0]; }
    T& back() const { ASSERT(_size > 0); return _data[_size-1]; }

    T* begin() { return _data; }
    T* end() { return _data + _size; }

    const T* begin() const { return _data; }
    const T* end() const { return _data + _size; }

    const T* cbegin() const { return _data; }
    const T* cend() const { return _data + _size; }

    T& operator[](u32 index) { ASSERT(index < _size); return _data[index]; }
    const T& operator[](u32 index) const { ASSERT(index < _size); return _data[index]; }

    void swap(u32 indexA, u32 indexB)
    {
        ASSERT(indexA < _size && indexB < _size);
        T a = _data[indexA];
        _data[indexA] = _data[indexB];
        _data[indexB] = a;
    }
};

template<typename T>
inline slice<T> make_slice(T* ptr, u32 size) { return slice<T>{ptr, size}; }

#define array_create(arena, type, capacity) array<type>::create(arena, capacity)
#define array_create_init(arena, type, init, ...) array<type>::create(arena, init, ## __VA_ARGS__)
#define array_create_copy(arena, srcarray, ...) array<type>::create_copy(arena, srcarray, ## __VA_ARGS__)


template<typename T>
struct array
{
    typedef slice<T> slice;

    // TODO(james): Should I add buffer overrun/underrun bounds checking?

    T* _data;
    u32 _size;
    u32 _capacity;

    static array<T>* create(memory_arena& arena, u32 capacity)
    {
        array<T>* arr = PushStruct(arena, array<T>);
        arr->_capacity = capacity;
        if(capacity) 
        {
            arr->_data = PushArray(arena, capacity, T);
        }
        return arr;
    }

    static array<T>* create_copy(memory_arena& arena, const array<T>* src, u32 capacity = 0)
    {
        array<T>* arr = PushStruct(arena, array<T>);
        arr->capacity = capacity ? capacity : src->_capacity;
        if(arr->capacity) 
        {
            arr->_data = PushArray(arena, capacity, T);
        }
        arr->_size = src->_size;
        CopyArray(arr->_size, src->_data, arr->_data);
        return arr;
    }

    template<size_t N>
    static array<T>* create(memory_arena& arena, const T (&init)[N], u32 capacity = N)
    {
        ASSERT(capacity >= N);
        array<T>* arr = PushStruct(arena, array<T>);
        if(capacity)
        {
            arr->_data = PushArray(arena, capacity, T);
        }
        arr->_capacity = capacity;
        CopyArray(N, init, arr->_data);
        arr->_size = N;
        return arr;
    }

    slice slice_leftof(u32 last)
    {
        return sub_slice(0, last);
    }

    slice sub_slice(u32 first, u32 size)
    {
        ASSERT((first + size) <= _size); 
        return slice { ._data = _data + first, ._size = size }; 
    }

    slice slice_rightof(u32 first)
    {
        return sub_slice(first, _size - first);
    }

    slice to_slice() { return slice { ._data = _data, ._size = _size }; }

    T* data() { return _data; }
    const T* data() const { return _data; }

    void set_size(u32 size) { ASSERT(size <= _capacity); _size = size; }

    u32 size() const { return _size; }
    u32 capacity() const { return _capacity; }
    b32 empty() const { return _size == 0; }
    b32 full() const { return _size == _capacity; }

    T& at(u32 index) { ASSERT(index < _size); return _data[index]; }
    T& front() { ASSERT(_size > 0); return _data[0]; }
    T& back() { ASSERT(_size > 0); return _data[_size-1]; }

    T* begin() { return _data; }
    T* end() { return _data + _size; }

    const T* begin() const { return _data; }
    const T* end() const { return _data + _size; }

    const T* cbegin() const { return _data; }
    const T* cend() const { return _data + _size; }

    T& operator[](u32 index) { ASSERT(index < _size); return _data[index]; }
    const T& operator[](u32 index) const { ASSERT(index < _size); return _data[index]; }

    void swap(u32 indexA, u32 indexB)
    {
        ASSERT(indexA < _size && indexB < _size);
        T a = _data[indexA];
        _data[indexA] = _data[indexB];
        _data[indexB] = a;
    }

    void push_back(const T& val) { ASSERT(_size+1 <= _capacity); _data[_size++] = val; }
    T pop_back() { ASSERT(_size > 0); return _data[_size--]; }

    void clear() { _size = 0; }

    T* insert(u32 beforeIndex, const T& val) 
    {
        ASSERT(_size+1 <= _capacity);
        ASSERT(beforeIndex <= _size);
        // NOTE(james): inserting into the array moves the item already at the index to the end to make room!
        _data[_size++] = _data[beforeIndex];
        _data[beforeIndex] = val;
        return _data + beforeIndex;
    }

    T* insert(T* before, const T& val)
    {
        ASSERT(_size+1 <= _capacity);
        ASSERT(before >= _data && before <= (_data + _size));     // make sure the pointer is actually in the array (or is the end())
        ASSERT(((PtrToUMM(before) - PtrToUMM(_data))%sizeof(T)) == 0);   // and make sure it is pointing to an actual index
        u32 index = (u32)((PtrToUMM(before) - PtrToUMM(_data))/sizeof(T));
        return insert(index, val);
    }

    T* erase(u32 index)
    {
        ASSERT(_size > 0);
        ASSERT(index < _size);
        u32 lastIndex = --_size;
        if(_size > 0)
        {
            // NOTE(james): if this is NOT the last element in the array, we'll
            // just move the last element into it's spot
            _data[index] = _data[lastIndex];
        }
        return _data + index;
    }

    T* erase(T* ptr) 
    { 
        ASSERT(ptr >= _data && ptr < (_data + _size));     // make sure the pointer is actually in the array
        ASSERT(((PtrToUMM(ptr) - PtrToUMM(_data))%sizeof(T)) == 0);   // and make sure it is pointing to an actual index
        u32 index = (u32)((PtrToUMM(ptr) - PtrToUMM(_data))/sizeof(T));
        return erase(index);
    }
};

#define hashtable_create(arena, type, size) hashtable<type>::create<size>(arena)

template<typename T>
struct hashtable
{
    enum : u32 { 
        end_pos = U32MAX
    };

    struct entry
    {
        u64 key;
        u32 next_idx;
        T value;

        T& operator*() { return value; }
    };

    struct find_result
    {
        u32 hash_index;
        u32 entry_index;
        u32 prev_entry_index;
    };

    T default_value;
    u32* _keys;
    array<entry> _entries;

    template<u32 SIZE>
    static hashtable<T>* create(memory_arena& arena)
    {
        hashtable<T>* ht = (hashtable<T>*)PushSize_(DEBUG_MEMORY_NAME("PushStruct") arena, sizeof(hashtable<T>));
        ht->_keys = PushArray(arena, SIZE, u32);
        for(umm i = 0; i < SIZE; ++i)
            ht->_keys[i] = end_pos; 
        ht->_entries = *array_create(arena, entry, SIZE);
        return ht;
    }

    u32 _hash_idx(u64 key) const { return key % _entries.capacity(); }

    u32 _add_entry(u64 key)
    {
        entry e = {};
        e.key = key;
        e.next_idx = end_pos;
        u32 index = _entries.size();
        _entries.push_back(e);
        return index;
    }

    find_result _find(u64 key) const
    {
        find_result fr = {};
        fr.hash_index = _hash_idx(key);
        fr.prev_entry_index = end_pos;
        fr.entry_index = _keys[fr.hash_index];

        while(fr.entry_index != end_pos)
        {
            if(_entries[fr.entry_index].key == key)
            {
                return fr;
            }
            fr.prev_entry_index = fr.entry_index;
            fr.entry_index = _entries[fr.entry_index].next_idx;
        }

        return fr;
    }

    void _erase(const find_result& fr)
    {
        if(fr.prev_entry_index == end_pos)
            _keys[fr.hash_index] = _entries[fr.entry_index].next_idx;
        else
            _entries[fr.prev_entry_index].next_idx = _entries[fr.entry_index].next_idx;

        _entries.erase(&_entries.back());
        const find_result last = _find(_entries.back().key);

        if(last.prev_entry_index != end_pos)
            _entries[last.prev_entry_index].next_idx = last.entry_index;
        else
            _keys[last.hash_index] = last.entry_index;
    }

    u32 _find_or_fail(u64 key) const
    {
        return _find(key).entry_index;
    }

    u32 _make(u64 key)
    {
        const find_result fr = _find(key);
        u32 index = _add_entry(key);

        if(fr.prev_entry_index == end_pos)
        {
            _keys[fr.hash_index] = index;
        }
        else
        {
            _entries[fr.prev_entry_index].next_idx = index;
        }

        _entries[index].next = fr.entry_index;  // NOTE(james): this should be end_pos
        return index;
    }

    u32 _find_or_make(u64 key)
    {
        const find_result fr = _find(key);
        if(fr.entry_index != end_pos)
            return fr.entry_index;

        u32 index = _add_entry(key);

        if(fr.prev_entry_index == end_pos)
        {
            _keys[fr.hash_index] = index;
        }
        else
        {
            _entries[fr.prev_entry_index].next_idx = index;
        }

        return index;
    }

    void _find_and_erase(u64 key)
    {
        const find_result fr = _find(key);
        if(fr.entry_index != end_pos)
        {
            _erase(fr);
        }
    }
    
    b32 contains(u64 key)
    {
        return _find_or_fail(key) != end_pos;
    }

    const T& get(u64 key) const
    {
        u32 index = _find_or_fail(key);
        ASSERT(index != end_pos);
        return index == end_pos ? default_value : _entries[index].value;
    }

    b32 try_get(u64 key, T* value)
    {
        u32 index = _find_or_fail(key);
        if(index != end_pos)
        {
            *value = _entries[index].value;
            return true;
        }

        return false;
    }
    
    void set(u64 key, const T& value)
    {
        u32 index = _find_or_make(key);
        _entries[index].value = value;
    }

    void erase(u64 key)
    {
        _find_and_erase(key);
    }

    void clear() 
    {
        for(u32 i = 0; i < _entries.capacity(); ++i) 
            _keys[i] = end_pos;
        _entries.clear(); 
    }

    b32 full() { return _entries.full(); }

    entry* begin() { return _entries.begin(); }
    entry* end() { return _entries.end(); }

    u32 capacity() { return _entries.capacity(); }
    u32 size() { return _entries.size(); }

    // NOTE(james): I'm intentionally leaving these out of the struct...
    //  I want the user to be aware whenever the hash table will add
    //  a new entry.  That said, this is the correct implementation
    #if 0
    T& operator[](u64 key)
    {
        u32 index = _find_or_make(key);
        return _entries[index].value;
    }
    
    const T& operator[](u64 key) const
    {
        return get(key);
    }
    #endif
};



namespace sort
{
    template<typename T> struct comparer {
         static bool lessthan(const T& a, const T& b) { return a < b; }
    };

    template<typename T, typename FNCOMPARE>
    void quickSort(slice<T> slice, FNCOMPARE compare)
    {
        // {5, 7, 3, 1, 0, 9, 2, 4, 8, 6}
        if(slice.size() > 1)
        {
            // get the pivot item
            T& pivot = slice.back();

            u32 indexSmaller = 0;
            for(u32 j = 0; j < slice.size()-1; ++j)
            {
                if(compare(slice[j], pivot))
                {
                    slice.swap(indexSmaller++, j);
                }
            }

            u32 pivotIndex = indexSmaller;
            slice.swap(pivotIndex, slice.size()-1);

            quickSort(slice.slice_leftof(pivotIndex), compare);
            quickSort(slice.slice_rightof(pivotIndex + 1), compare);
        }
    }

    template<typename T, typename FNCOMPARE>
    void quickSort(array<T>& collection, FNCOMPARE compare)
    {
        quickSort(collection.to_slice(), compare);
    }

    template<typename T>
    void quickSort(array<T>& collection)
    {
        quickSort(collection.to_slice(), comparer<T>::lessthan);
    }

    template<typename T, typename FNCOMPARE>
    void _heapify(slice<T> slice, u32 root, FNCOMPARE compare)
    {
        // find the largest root, left child and right child
        u32 largest = root;
        u32 left = 2 * root + 1;
        u32 right = 2 * root + 2;

        if(left < slice.size() && compare(slice[largest], slice[left]))
        {
            largest = left;
        }

        if(right < slice.size() && compare(slice[largest], slice[right]))
        {
            largest = right;
        }

        if(largest != root)
        {
            // swap and recursively heapify if the root is not the largest
            slice.swap(largest, root);
            _heapify(slice, largest, compare);
        }
    }

    template<typename T, typename FNCOMPARE>
    void heapSort(array<T>& collection, FNCOMPARE compare)
    {
        // build max heap
        for(i32 i = collection.size() / 2 - 1; i >= 0; --i)
        {
            _heapify(collection.to_slice(), i, compare);
        }

        // actually sort
        for(i32 i = collection.size()-1; i > 0; --i)
        {
            collection.swap(0, i);

            // heapify the root to get the next largest item
            _heapify(collection.slice_leftof(i), 0, compare);
        }
    }

    template<typename T>
    void heapSort(array<T>& collection)
    {
        heapSort(collection, comparer<T>::lessthan);
    }
};


#if TEST_COLLECTIONS
#define EXPECT(cond) ASSERT(cond); if(!(cond)) return false
b32 TestArray()
{
    memory_arena scratch = {};
    array<u32>& arr = *array_create(scratch, u32, 10);
    EXPECT(arr.data());
    EXPECT(arr.size() == 0);
    EXPECT(arr.capacity() == 10);

    arr.push_back(6);
    arr.push_back(7);
    arr.push_back(3);
    arr.push_back(14);
    arr.push_back(76);
    arr.push_back(22);
    EXPECT(arr.size() == 6);

    arr.insert(arr.begin(), 42);
    EXPECT(arr.size() == 7);
    EXPECT(arr[0] == 42);
    EXPECT(arr[6] == 6);
    arr.insert(arr.end(), 55);
    EXPECT(arr.size() == 8);
    EXPECT(arr[7] == 55);
    arr.insert(6, 67);
    EXPECT(arr.size() == 9);
    EXPECT(arr[6] == 67);
    EXPECT(arr[8] == 6);
    arr.push_back(100);

    int count = 0;
    for(auto it : arr)
    {
        count++;
    }
    EXPECT(count == 10);

    auto next = arr.erase(arr.begin());
    
    while(!arr.empty()) next = arr.erase(next);

    Clear(scratch);
    return true;
}

struct test_sort_item
{
    u32 key;
    const char* name;

    bool operator < (const test_sort_item& rhs) const {
        return strcmp(name, rhs.name) < 0;
    }
    
    bool operator <= (const test_sort_item& rhs) const {
        return strcmp(name, rhs.name) <= 0;
    }

    bool operator >= (const test_sort_item& rhs) const {
        return strcmp(name, rhs.name) >= 0;
    }

    bool operator > (const test_sort_item& rhs) const {
        return strcmp(name, rhs.name) > 0;
    }
};
#define TESTSORTITEM(name) { C_HASH(name), #name }

b32 TestSort()
{
    memory_arena scratch = {};
    array<u32>& arr = *array_create_init(scratch, u32, {5, 7, 3, 1, 0, 9, 2, 4, 8, 6});

    sort::quickSort(arr);

    u32 prev = 0;
    for(auto it : arr)
    {
        EXPECT(prev <= it);
        prev = it;
    }

    array<test_sort_item>& arr2 = *array<test_sort_item>::create(scratch, {TESTSORTITEM(item1), TESTSORTITEM(item2), TESTSORTITEM(item3)});

    sort::quickSort(arr2, [](const test_sort_item& a, const test_sort_item& b) { return strcmp(a.name, b.name) > 0; });

    test_sort_item prevItem = arr2[0];
    for(auto& it : arr2)
    {
        EXPECT(prevItem >= it);
        prevItem = it;
    }

    // should reverse it..
    sort::heapSort(arr2);

    prevItem = arr2[0];
    for(auto& it : arr2)
    {
        EXPECT(prevItem <= it);
        prevItem = it;
    }

    Clear(scratch);
    return true;
}

b32 TestHashTable()
{
    memory_arena scratch = {};

    auto& ht = *hashtable_create(scratch, u64, 1024);
    for(u64 i = 0; i < 1024; ++i)
    {
        ht.set(i, i);
    }

    for(u64 i = 0; i < 1024; ++i)
    {
        u64 v = ht.get(i);
        EXPECT(v == i);
    }
    EXPECT(ht.contains(40));
    ht.erase(40);
    EXPECT(!ht.contains(40));
    EXPECT(ht.contains(68));
    EXPECT(ht.contains(81));

    ht.clear();
    EXPECT(ht.size() == 0);

    Clear(scratch);
    return true;
}

b32 TestCollections()
{
    b32 passed = true;
    passed &= TestArray();
    passed &= TestSort();
    passed &= TestHashTable();

    return passed;
}
#endif