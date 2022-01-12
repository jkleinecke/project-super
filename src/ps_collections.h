
// define TEST_COLLECTIONS to include the testing routines
#define TEST_COLLECTIONS 1

// NOTE(james): intended to be used as a POD container.  Not suitable as a replacement for std::vector<>!!
// Does not automatically grow!
// Does not perform runtime bounds checking or throw, will only ASSERT! YOU still have to not overrun the end of the array!
// insert() moves the element at the index to the end of the array to make room rather than moving all the elements down a position
// erase() moves the element at the end of the array to the slot being erased

template<typename T>
struct span
{
    T* _data;
    u32 _size;

    span span_leftof(u32 last)
    {
        return sub_span(0, last);
    }

    span sub_span(u32 first, u32 size)
    {
        ASSERT((first + size) <= _size); 
        return span { ._data = _data + first, ._size = size }; 
    }

    span span_rightof(u32 first)
    {
        return sub_span(first, _size - first);
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
struct array
{
    typedef span<T> span;

    // TODO(james): Should I add buffer overrun/underrun bounds checking?

    T* _data;
    u32 _size;
    u32 _capacity;

    static array<T>* create(memory_arena& arena, u32 capacity)
    {
        ASSERT(capacity > 0);
        array<T>* arr = PushStruct(arena, array<T>);
        arr->_data = PushArray(arena, capacity, T);
        arr->_capacity = capacity;
        return arr;
    }

    template<size_t N>
    static array<T>* create(memory_arena& arena, const T (&init)[N], u32 capacity = N)
    {
        ASSERT(capacity >= N);
        array<T>* arr = PushStruct(arena, array<T>);
        arr->_data = PushArray(arena, capacity, T);
        arr->_capacity = capacity;
        CopyArray(N, init, arr->_data);
        arr->_size = N;
        return arr;
    }

    span span_leftof(u32 last)
    {
        return sub_span(0, last);
    }

    span sub_span(u32 first, u32 size)
    {
        ASSERT((first + size) <= _size); 
        return span { ._data = _data + first, ._size = size }; 
    }

    span span_rightof(u32 first)
    {
        return sub_span(first, _size - first);
    }

    span to_span() { return span { ._data = _data, ._size = _size }; }

    T* data() { return _data; }
    const T* data() const { return _data; }

    u32 size() const { return _size; }
    u32 capacity() const { return _capacity; }
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

    void push_back(const T& val) { ASSERT(_size+1 <= _capacity); _data[_size++] = val; }
    T pop_back() { ASSERT(_size > 0); return _data[_size--]; }

    void clear() { size = 0; }

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

    operator span&() { return (span&)*this; }
};

namespace sort
{
    template<typename T> struct comparer {
         static bool lessthan(const T& a, const T& b) { return a < b; }
    };

    template<typename T, typename FNCOMPARE>
    void quickSort(span<T> span, FNCOMPARE compare)
    {
        // {5, 7, 3, 1, 0, 9, 2, 4, 8, 6}
        if(span.size() > 1)
        {
            // get the pivot item
            T& pivot = span.back();

            u32 indexSmaller = 0;
            for(u32 j = 0; j < span.size()-1; ++j)
            {
                if(compare(span[j], pivot))
                {
                    span.swap(indexSmaller++, j);
                }
            }

            u32 pivotIndex = indexSmaller;
            span.swap(pivotIndex, span.size()-1);

            quickSort(span.span_leftof(pivotIndex), compare);
            quickSort(span.span_rightof(pivotIndex + 1), compare);
        }
    }

    template<typename T, typename FNCOMPARE>
    // void quickSort(array<T>& collection, bool (*compare)(const T& a, const T& b) = &comparer<T>::lessthan)
    void quickSort(array<T>& collection, FNCOMPARE compare)
    {
        quickSort(collection.to_span(), compare);
    }

    template<typename T>
    void quickSort(array<T>& collection)
    {
        quickSort(collection.to_span(), comparer<T>::lessthan);
    }

    template<typename T, typename FNCOMPARE>
    void _heapify(span<T> span, u32 root, FNCOMPARE compare)
    {
        // find the largest root, left child and right child
        u32 largest = root;
        u32 left = 2 * root + 1;
        u32 right = 2 * root + 2;

        if(left < span.size() && compare(span[largest], span[left]))
        {
            largest = left;
        }

        if(right < span.size() && compare(span[largest], span[right]))
        {
            largest = right;
        }

        if(largest != root)
        {
            // swap and recursively heapify if the root is not the largest
            span.swap(largest, root);
            _heapify(span, largest, compare);
        }
    }

    template<typename T, typename FNCOMPARE>
    void heapSort(array<T>& collection, FNCOMPARE compare)
    {
        // build max heap
        for(i32 i = collection.size() / 2 - 1; i >= 0; --i)
        {
            _heapify(collection.to_span(), i, compare);
        }

        // actually sort
        for(i32 i = collection.size()-1; i > 0; --i)
        {
            collection.swap(0, i);

            // heapify the root to get the next largest item
            _heapify(collection.span_leftof(i), 0, compare);
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
    array<u32>& arr = *array<u32>::create(scratch, 10);
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
    array<u32>& arr = *array<u32>::create(scratch, {5, 7, 3, 1, 0, 9, 2, 4, 8, 6});

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

b32 TestCollections()
{
    b32 passed = true;
    passed &= TestArray();
    passed &= TestSort();

    return passed;
}
#endif