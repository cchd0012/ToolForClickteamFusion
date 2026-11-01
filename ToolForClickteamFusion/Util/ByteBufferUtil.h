#pragma once

#include "../Common.h"
#include <stdexcept>

// Bounds checked little endian reader / writer for in-memory chunk data.

namespace CTT {

	namespace ByteBufferUtil {

		using namespace std;

		typedef vector<BYTE> ByteBuffer;

		class ByteBufferError : public runtime_error
		{
		public:
			explicit ByteBufferError(const string& message) : runtime_error(message) {}
		};


		class ByteBufferReader
		{
		public:
			ByteBufferReader(const BYTE* data, size_t size, size_t pos = 0) : buffer(data), bufferSize(size), position(pos) {}
			explicit ByteBufferReader(const ByteBuffer& data, size_t pos = 0) : buffer(data.empty() ? NULL : &data[0]), bufferSize(data.size()), position(pos) {}

			size_t tell() const { return position; }
			size_t size() const { return bufferSize; }
			size_t remaining() const { return (position <= bufferSize) ? (bufferSize - position) : 0; }
			void seek(size_t pos) { position = pos; }
			void skip(size_t count) { need(count); position += count; }

			BYTE readU8() { need(1); return buffer[position++]; }
			INT8 readI8() { return (INT8)readU8(); }
			UINT16 readU16()
			{
				need(2);
				UINT16 value = (UINT16)(buffer[position] | (buffer[position + 1] << 8));
				position += 2;
				return value;
			}
			INT16 readI16() { return (INT16)readU16(); }
			UINT32 readU32()
			{
				need(4);
				UINT32 value = (UINT32)buffer[position] | ((UINT32)buffer[position + 1] << 8) | ((UINT32)buffer[position + 2] << 16) | ((UINT32)buffer[position + 3] << 24);
				position += 4;
				return value;
			}
			INT32 readI32() { return (INT32)readU32(); }

			ByteBuffer readBytes(size_t count)
			{
				need(count);
				ByteBuffer out(buffer + position, buffer + position + count);
				position += count;
				return out;
			}

			// null terminated UTF-16LE string (the terminator is consumed)
			wstring readWString()
			{
				wstring value;
				while (true) {
					UINT16 c = readU16();
					if (c == 0) {
						break;
					}
					value.push_back((wchar_t)c);
				}
				return value;
			}

		private:
			void need(size_t count) const
			{
				if ((position > bufferSize) || (count > (bufferSize - position))) {
					throw ByteBufferError("read past end of buffer");
				}
			}

			const BYTE* buffer;
			size_t bufferSize;
			size_t position;
		};


		class ByteBufferWriter
		{
		public:
			size_t size() const { return buffer.size(); }
			ByteBuffer& getBuffer() { return buffer; }
			const ByteBuffer& getBuffer() const { return buffer; }

			void writeU8(BYTE value) { buffer.push_back(value); }
			void writeI8(INT8 value) { buffer.push_back((BYTE)value); }
			void writeU16(UINT16 value)
			{
				buffer.push_back((BYTE)(value & 0xFF));
				buffer.push_back((BYTE)(value >> 8));
			}
			void writeI16(INT16 value) { writeU16((UINT16)value); }
			void writeU32(UINT32 value)
			{
				buffer.push_back((BYTE)(value & 0xFF));
				buffer.push_back((BYTE)((value >> 8) & 0xFF));
				buffer.push_back((BYTE)((value >> 16) & 0xFF));
				buffer.push_back((BYTE)((value >> 24) & 0xFF));
			}
			void writeI32(INT32 value) { writeU32((UINT32)value); }
			void writeBytes(const ByteBuffer& data) { buffer.insert(buffer.end(), data.begin(), data.end()); }
			void writeBytes(const BYTE* data, size_t count) { buffer.insert(buffer.end(), data, data + count); }
			void writeWString(const wstring& value)
			{
				for (size_t i = 0; i < value.size(); i++) {
					writeU16((UINT16)value[i]);
				}
				writeU16(0);
			}

			void patchU16(size_t pos, UINT16 value)
			{
				buffer[pos] = (BYTE)(value & 0xFF);
				buffer[pos + 1] = (BYTE)(value >> 8);
			}
			void patchI16(size_t pos, INT16 value) { patchU16(pos, (UINT16)value); }
			void patchU32(size_t pos, UINT32 value)
			{
				buffer[pos] = (BYTE)(value & 0xFF);
				buffer[pos + 1] = (BYTE)((value >> 8) & 0xFF);
				buffer[pos + 2] = (BYTE)((value >> 16) & 0xFF);
				buffer[pos + 3] = (BYTE)((value >> 24) & 0xFF);
			}
			void patchI32(size_t pos, INT32 value) { patchU32(pos, (UINT32)value); }

		private:
			ByteBuffer buffer;
		};

	}

}
