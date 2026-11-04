#include "Md5.h"

#include <algorithm>
#include <cstring>

namespace AcbCore
{
	namespace
	{
		const uint32_t Md5K[64] = {
			0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
			0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
			0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
			0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
			0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
			0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
			0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
			0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };
		const uint32_t Md5Shift[64] = {
			7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
			5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
			4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
			6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };

		inline uint32_t RotateLeft(uint32_t X, uint32_t C) { return (X << C) | (X >> (32 - C)); }
	}

	Md5::Md5()
	{
		State[0] = 0x67452301;
		State[1] = 0xefcdab89;
		State[2] = 0x98badcfe;
		State[3] = 0x10325476;
	}

	void Md5::Transform(const uint8_t Block[64])
	{
		uint32_t M[16];
		for (int Index = 0; Index < 16; ++Index)
		{
			M[Index] = ReadLE32(Block + Index * 4);
		}
		uint32_t A = State[0], B = State[1], C = State[2], D = State[3];
		for (uint32_t Index = 0; Index < 64; ++Index)
		{
			uint32_t F, G;
			if (Index < 16) { F = (B & C) | (~B & D); G = Index; }
			else if (Index < 32) { F = (D & B) | (~D & C); G = (5 * Index + 1) % 16; }
			else if (Index < 48) { F = B ^ C ^ D; G = (3 * Index + 5) % 16; }
			else { F = C ^ (B | ~D); G = (7 * Index) % 16; }
			const uint32_t Temp = D;
			D = C;
			C = B;
			B = B + RotateLeft(A + F + Md5K[Index] + M[G], Md5Shift[Index]);
			A = Temp;
		}
		State[0] += A;
		State[1] += B;
		State[2] += C;
		State[3] += D;
	}

	void Md5::Update(const void* Data, size_t Count)
	{
		const uint8_t* P = static_cast<const uint8_t*>(Data);
		Length += Count;
		while (Count > 0)
		{
			const size_t Take = std::min(Count, size_t(64) - BufferUsed);
			std::memcpy(Buffer + BufferUsed, P, Take);
			BufferUsed += Take;
			P += Take;
			Count -= Take;
			if (BufferUsed == 64)
			{
				Transform(Buffer);
				BufferUsed = 0;
			}
		}
	}

	std::array<uint8_t, 16> Md5::Finish()
	{
		const uint64_t BitLength = Length * 8;
		const uint8_t Pad80 = 0x80;
		const uint8_t Zero = 0;
		Update(&Pad80, 1);
		while (BufferUsed != 56)
		{
			Update(&Zero, 1);
		}
		uint8_t LengthBytes[8];
		for (int Index = 0; Index < 8; ++Index)
		{
			LengthBytes[Index] = uint8_t(BitLength >> (8 * Index));
		}
		Update(LengthBytes, 8);
		std::array<uint8_t, 16> Digest;
		for (int Index = 0; Index < 4; ++Index)
		{
			for (int Byte = 0; Byte < 4; ++Byte)
			{
				Digest[size_t(Index * 4 + Byte)] = uint8_t(State[Index] >> (8 * Byte));
			}
		}
		return Digest;
	}
}
