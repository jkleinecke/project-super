

typedef umm PsRingPosition;

struct ps_ringmemory_stream
{
	void* 			base_pointer;
	mem_size 		max_size;
	PsRingPosition	write_cursor;
	PsRingPosition  read_cursor;
};

internal
ps_ringmemory_stream psMakeRingMemoryStream(void* baseptr, mem_size max_size)
{
	ps_ringmemory_stream mem_stream{};
	mem_stream.base_pointer = baseptr;
	mem_stream.max_size = max_size;
	mem_stream.write_cursor = 0;
	mem_stream.read_cursor = 0;

	return mem_stream;
}

internal
mem_size psRingMemoryWrite(ps_ringmemory_stream& stream, const void* src, mem_size writeSize)
{
	PsRingPosition next_pos = stream.write_cursor + writeSize;
	void* dest = OffsetPtr(stream.base_pointer, stream.write_cursor);

	if(next_pos < stream.max_size)
	{
		// all we need is a simple copy here
		Copy(writeSize, src, dest);
	}
	else
	{
		// since we'd write past the end of the buffer, we have to write in two parts
		// first write as much as we can to the end of the buffer
		mem_size size_to_end = stream.max_size - stream.write_cursor;
		Copy(size_to_end, src, dest);

		// now write the remaining bytes to the base
		mem_size remaining_size = writeSize - size_to_end;
		void* remaining_src = OffsetPtr(src, size_to_end);
		Copy(remaining_size, remaining_src, stream.base_pointer);

		// the next position is now where we stopped writing
		next_pos = remaining_size;
	}

	stream.write_cursor = next_pos;

	return writeSize;
}

internal
mem_size psRingMemoryRead(ps_ringmemory_stream& stream, mem_size sizeToRead, void* outbuffer, mem_size maxOutBufferSize)
{
	mem_size readSize = sizeToRead;
	IFF(sizeToRead > maxOutBufferSize, readSize = maxOutBufferSize);

	PsRingPosition next_pos = stream.read_cursor + readSize;
	void* src = OffsetPtr(stream.base_pointer, stream.read_cursor);

	if(next_pos < stream.max_size)
	{
		// just a simple copy to the outbuffer
		Copy(readSize, src, outbuffer);
	}
	else
	{
		// need to make 2 copies
		
		// first read to the end of the stream
		mem_size size_to_end = stream.max_size - stream.read_cursor;
		Copy(size_to_end, src, outbuffer);

		// now from the beginning for the remaining
		mem_size remaining_size = readSize - size_to_end;
		void* outbuffer_offsetptr = OffsetPtr(outbuffer, size_to_end);
		Copy(remaining_size, stream.base_pointer, outbuffer_offsetptr);
		next_pos = remaining_size;
	}

	stream.read_cursor = next_pos;

	return readSize;
}