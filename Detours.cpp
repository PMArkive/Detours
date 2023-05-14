﻿#include "Detours.h"

// Default
#include <tchar.h>

// Advanced
#include <intrin.h>

// STL
#include <unordered_map>

// ----------------------------------------------------------------
// Detours
// ----------------------------------------------------------------

namespace Detours {

	// ----------------------------------------------------------------
	// Namespaces
	// ----------------------------------------------------------------

	using namespace Codec;
	using namespace Hexadecimal;
	using namespace Scan;
	using namespace Memory;
	using namespace Exception;
	using namespace Hook;

	// ----------------------------------------------------------------
	// Storage
	// ----------------------------------------------------------------

	static std::unordered_map<void*, std::unique_ptr<Protection>> g_Protections;
	static std::unordered_map<void*, std::unique_ptr<MemoryHook>> g_MemoryHooks;

	// ----------------------------------------------------------------
	// KUSER_SHARED_DATA
	// ----------------------------------------------------------------

	const volatile KUSER_SHARED_DATA& KUserSharedData = *reinterpret_cast<PKUSER_SHARED_DATA>(0x7FFE0000);

	// ----------------------------------------------------------------
	// PEB
	// ----------------------------------------------------------------

	const PPEB GetPEB() {
#ifdef _M_X64
		return reinterpret_cast<PPEB>(__readgsqword(0x60));
#elif _M_IX86
		return reinterpret_cast<PPEB>(__readfsdword(0x30));
#endif
	}

	// ----------------------------------------------------------------
	// TEB
	// ----------------------------------------------------------------

	const PTEB GetTEB() {
#ifdef _M_X64
		return reinterpret_cast<PTEB>(__readgsqword(0x30));
#elif _M_IX86
		return reinterpret_cast<PTEB>(__readfsdword(0x18));
#endif
	}

	// ----------------------------------------------------------------
	// Codec
	// ----------------------------------------------------------------

	namespace Codec {

		// ----------------------------------------------------------------
		// Encode
		// ----------------------------------------------------------------

		int Encode(unsigned short unCodePage, const char* const szText, wchar_t* szBuffer, const int nBufferSize) {
			if (!unCodePage || !szText) {
				return -1;
			}

			const size_t unTextLength = strnlen_s(szText, 0x1000);
			if (!unTextLength) {
				return -1;
			}

			int nSize = MultiByteToWideChar(unCodePage, NULL, szText, static_cast<int>(unTextLength), nullptr, 0);
			if (nSize <= 0) {
				return -1;
			}

			if (!szBuffer) {
				return nSize;
			}

			return MultiByteToWideChar(unCodePage, NULL, szText, static_cast<int>(unTextLength), szBuffer, nBufferSize);
		}

		// ----------------------------------------------------------------
		// Decode
		// ----------------------------------------------------------------

		int Decode(unsigned short unCodePage, const wchar_t* const szText, char* szBuffer, const int nBufferSize) {
			if (!unCodePage || !szText) {
				return -1;
			}

			const size_t unTextLength = wcsnlen_s(szText, 0x1000);
			if (!unTextLength) {
				return -1;
			}

			int nSize = WideCharToMultiByte(unCodePage, NULL, szText, static_cast<int>(unTextLength), nullptr, 0, nullptr, nullptr);
			if (nSize <= 0) {
				return -1;
			}

			if (!szBuffer) {
				return nSize;
			}

			return WideCharToMultiByte(unCodePage, NULL, szText, static_cast<int>(unTextLength), szBuffer, nBufferSize, nullptr, nullptr);
		}
	}

	// ----------------------------------------------------------------
	// Hexadecimal
	// ----------------------------------------------------------------

	namespace Hexadecimal {

		// ----------------------------------------------------------------
		// Encode
		// ----------------------------------------------------------------

		static const char g_pEncodeTable[16] = {
			'0', '1', '2', '3',
			'4', '5', '6', '7',
			'8', '9', 'A', 'B',
			'C', 'D', 'E', 'F'
		};

		bool EncodeA(const void* const pData, const size_t unSize, char* szHex, const unsigned char unIgnoredByte) {
			if (!pData || !unSize || !szHex) {
				return false;
			}

			const unsigned char* pByteData = reinterpret_cast<const unsigned char* const>(pData);
			for (size_t i = 0; i < unSize; i++) {
				const unsigned char unByte = pByteData[i] ? pByteData[i] : unIgnoredByte;
				*szHex++ = g_pEncodeTable[unByte >> 4];
				*szHex++ = g_pEncodeTable[unByte & 15];
			}

			return true;
		}

		bool EncodeW(const void* const pData, const size_t unSize, wchar_t* szHex, const unsigned char unIgnoredByte) {
			if (!pData || !unSize || !szHex) {
				return false;
			}

			const unsigned char* pByteData = reinterpret_cast<const unsigned char* const>(pData);
			for (size_t i = 0; i < unSize; i++) {
				const unsigned char unByte = pByteData[i] ? pByteData[i] : unIgnoredByte;
				*szHex++ = static_cast<wchar_t>(g_pEncodeTable[unByte >> 4]);
				*szHex++ = static_cast<wchar_t>(g_pEncodeTable[unByte & 15]);
			}

			return true;
		}

#ifdef UNICODE
		bool Encode(const void* const pData, const size_t unSize, wchar_t* szHex, const unsigned char unIgnoredByte) {
			return EncodeW(pData, unSize, szHex, unIgnoredByte);
		}
#else
		bool Encode(const void* const pData, const size_t unSize, char* szHex, const unsigned char unIgnoredByte) {
			return EncodeA(pData, unSize, szHex, unIgnoredByte);
		}
#endif

		// ----------------------------------------------------------------
		// Decode
		// ----------------------------------------------------------------

		static const unsigned char g_pDecodeTableLow[256] = {
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
		};

		static const unsigned char g_pDecodeTableHigh[256] = {
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
			0x80, 0x90, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
		};

		bool DecodeA(const char* const szHex, void* pData, const unsigned char unIgnoredByte) {
			if (!szHex || !pData) {
				return false;
			}

			const size_t unHexLength = strnlen_s(szHex, 0x1000);
			if (!unHexLength) {
				return false;
			}

			for (size_t i = 0; i < unHexLength; i += 2) {
				const unsigned char unHigh = szHex[i];
				const unsigned char unLow = szHex[i + 1];
				const unsigned char unByte = g_pDecodeTableHigh[unHigh] | g_pDecodeTableLow[unLow];
				if (unByte == unIgnoredByte) {
					i += 2;
					continue;
				}
				reinterpret_cast<unsigned char*>(pData)[i / 2] = unByte;
			}

			return true;
		}

		bool DecodeW(const wchar_t* const szHex, void* pData, const unsigned char unIgnoredByte) {
			if (!szHex || !pData) {
				return false;
			}

			const size_t unHexLength = wcsnlen_s(szHex, 0x1000);
			if (!unHexLength) {
				return false;
			}

			for (size_t i = 0; i < unHexLength; i += 2) {
				const unsigned char unHigh = static_cast<unsigned char>(szHex[i] & 0xFF);
				const unsigned char unLow = static_cast<unsigned char>(szHex[i + 1] & 0xFF);
				const unsigned char unByte = g_pDecodeTableHigh[unHigh] | g_pDecodeTableLow[unLow];
				if (unByte == unIgnoredByte) {
					i += 2;
					continue;
				}
				reinterpret_cast<unsigned char*>(pData)[i / 2] = unByte;
			}

			return true;
		}

#ifdef UNICODE
		bool Decode(const wchar_t* const szHex, void* pData, const unsigned char unIgnoredByte) {
			return DecodeW(szHex, pData, unIgnoredByte);
		}
#else
		bool Decode(const char* const szHex, void* pData, const unsigned char unIgnoredByte) {
			return DecodeA(szHex, pData, unIgnoredByte);
		}
#endif
	}

	// ----------------------------------------------------------------
	// Scan
	// ----------------------------------------------------------------

	namespace Scan {

		// ----------------------------------------------------------------
		// __align_up
		// ----------------------------------------------------------------

		template <typename T>
		static const T inline __align_up(const T unValue, const T unAlignment) {
			return (unValue + unAlignment - 1) & ~(unAlignment - 1);
		};

		// ----------------------------------------------------------------
		// __bit_scan_forward
		// ----------------------------------------------------------------

		template <typename T>
		static const T inline __bit_scan_forward(const T unValue) {
			for (unsigned char i = 0; i < (sizeof(T) * CHAR_BIT); ++i) {
				if (((unValue >> i) & 1) != 0) {
					return i;
				}
			}
			return sizeof(T) * CHAR_BIT;
		}

		// ----------------------------------------------------------------
		// FindSection
		// ----------------------------------------------------------------

		bool FindSection(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, void** pAddress, size_t* pSize) {
			if (!hModule) {
				return false;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_FILE_HEADER pFH = &(pNTHs->FileHeader);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			const PIMAGE_SECTION_HEADER pFirstSection = reinterpret_cast<PIMAGE_SECTION_HEADER>(reinterpret_cast<char*>(pOH) + pFH->SizeOfOptionalHeader);
			const WORD unNumberOfSections = pFH->NumberOfSections;
			const size_t unFileAlignment = static_cast<size_t>(pOH->FileAlignment);
			for (WORD i = 0; i < unNumberOfSections; ++i) {
				if (memcmp(SectionName.data(), pFirstSection[i].Name, 8) == 0) {
					if (pAddress) {
						*pAddress = reinterpret_cast<void*>(reinterpret_cast<char*>(hModule) + pFirstSection[i].VirtualAddress);
					}

					if (pSize) {
						*pSize = __align_up(static_cast<size_t>(pFirstSection[i].SizeOfRawData), unFileAlignment);
					}

					return true;
				}
			}

			return false;
		}

		// ----------------------------------------------------------------
		// FindSectionPOGO
		// ----------------------------------------------------------------

		typedef struct _IMAGE_POGO_BLOCK {
			DWORD m_unRVA;
			DWORD m_unSize;
			char m_pName[1];
		} IMAGE_POGO_BLOCK, *PIMAGE_POGO_BLOCK;

		typedef struct _IMAGE_POGO_INFO {
			DWORD m_Signature; // 0x4C544347 = 'LTCG'
			IMAGE_POGO_BLOCK m_pBlocks[1];
		} IMAGE_POGO_INFO, *PIMAGE_POGO_INFO;

		bool FindSectionPOGO(const HMODULE hModule, const char* const szSectionName, void** pAddress, size_t* pSize) {
			if (!hModule || !szSectionName) {
				return false;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			const IMAGE_DATA_DIRECTORY DebugDD = pOH->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
			if (!DebugDD.Size) {
				return false;
			}

			const PIMAGE_DEBUG_DIRECTORY pDebugDirectory = reinterpret_cast<PIMAGE_DEBUG_DIRECTORY>(reinterpret_cast<char*>(hModule) + DebugDD.VirtualAddress);
			for (DWORD i = 0; i < DebugDD.Size / sizeof(IMAGE_DEBUG_DIRECTORY); ++i) {
				if (pDebugDirectory[i].Type != IMAGE_DEBUG_TYPE_POGO) {
					continue;
				}

				const PIMAGE_POGO_INFO pPI = reinterpret_cast<PIMAGE_POGO_INFO>(reinterpret_cast<char*>(hModule) + pDebugDirectory[i].AddressOfRawData);
				if (pPI->m_Signature != 0x4C544347) {
					continue;
				}

				PIMAGE_POGO_BLOCK pBlock = pPI->m_pBlocks;
				while (pBlock->m_unRVA != 0) {
					const size_t unNameLength = strnlen_s(pBlock->m_pName, 0x1000) + 1; // FIXME: Unsafe.
					size_t unBlockSize = sizeof(DWORD) * 2 + unNameLength;
					if (unBlockSize & 3) {
						unBlockSize += (4 - (unBlockSize & 3));
					}

					if (strncmp(szSectionName, pBlock->m_pName, 0x1000) == 0) {  // FIXME: Unsafe.
						if (pAddress) {
							*pAddress = reinterpret_cast<void*>(reinterpret_cast<char*>(hModule) + pBlock->m_unRVA);
						}

						if (pSize) {
							*pSize = static_cast<size_t>(pBlock->m_unSize);
						}

						return true;
					}

					pBlock = reinterpret_cast<PIMAGE_POGO_BLOCK>(reinterpret_cast<char*>(pBlock) + unBlockSize);
				}
			}

			return false;
		}

		// ----------------------------------------------------------------
		// FindSignature (Native)
		// ----------------------------------------------------------------

		const void* const FindSignatureNative(const void* const pAddress, const size_t unSize, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!pAddress || !unSize || !szSignature) {
				return nullptr;
			}

			const size_t unSignatureLength = strnlen_s(szSignature, 0x1000);
			if (!unSignatureLength || (unSize <= unSignatureLength)) {
				return nullptr;
			}

			const unsigned char* const pData = reinterpret_cast<const unsigned char* const>(pAddress);
			const unsigned char* const pSignature = reinterpret_cast<const unsigned char* const>(szSignature);

			for (size_t unIndex = 0; unIndex < unSize; ++unIndex) {
				size_t unSignatureIndex = 0;
				for (; unSignatureIndex < unSignatureLength; ++unSignatureIndex) {
					const unsigned char unSignatureByte = pSignature[unSignatureIndex];
					if (unSignatureByte == unIgnoredByte) {
						continue;
					} else if (pData[unIndex + unSignatureIndex] != unSignatureByte) {
						break;
					}
				}
				if (unSignatureIndex == unSignatureLength) {
					return pData + unIndex;
				}
			}

			return nullptr;
		}

		const void* const FindSignatureNative(const HMODULE hModule, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSignature) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			return FindSignatureNative(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureNative(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSignature) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSection(hModule, SectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindSignatureNative(pAddress, unSize, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureNative(const HMODULE hModule, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSectionName || !szSignature) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSectionPOGO(hModule, szSectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindSignatureNative(pAddress, unSize, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureNativeA(const char* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureNative(hMod, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureNativeA(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureNative(hMod, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureNativeA(const char* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSectionName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureNative(hMod, szSectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureNativeW(const wchar_t* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureNative(hMod, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureNativeW(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureNative(hMod, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureNativeW(const wchar_t* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSectionName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureNative(hMod, szSectionName, szSignature, unIgnoredByte);
		}

#ifdef UNICODE
		const void* const FindSignatureNative(const wchar_t* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureNativeW(szModuleName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureNative(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureNativeW(szModuleName, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureNative(const wchar_t* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureNativeW(szModuleName, szSectionName, szSignature, unIgnoredByte);
		}
#else
		const void* const FindSignatureNative(const char* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureNativeA(szModuleName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureNative(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureNativeA(szModuleName, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureNative(const char* const szModuleName, const char* const szSectionName,  const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureNativeA(szModuleName, szSectionName, szSignature, unIgnoredByte);
		}
#endif

		// ----------------------------------------------------------------
		// FindSignature (SSE2)
		// ----------------------------------------------------------------

		const void* const FindSignatureSSE2(const void* const pAddress, const size_t unSize, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!pAddress || !unSize || !szSignature) {
				return nullptr;
			}

			const size_t unSignatureLength = strnlen_s(szSignature, 0x1000);
			if (!unSignatureLength || (unSize < unSignatureLength)) {
				return nullptr;
			}

			const unsigned char* const pData = reinterpret_cast<const unsigned char* const>(pAddress);
			const unsigned char* const pSignature = reinterpret_cast<const unsigned char* const>(szSignature);

			const size_t unDataBytesCycles = unSize / 16;
			for (size_t unCycle = 0; unCycle < unDataBytesCycles; ++unCycle) {
				unsigned __int16 unFound = 0xFFFFui16;
				for (size_t unSignatureIndex = 0; (unSignatureIndex < unSignatureLength) && (unFound != 0); ++unSignatureIndex) {
					const unsigned char unSignatureByte = pSignature[unSignatureIndex];
					if (unSignatureByte == unIgnoredByte) {
						continue;
					} else {
						const __m128i xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pData + unCycle * 16 + unSignatureIndex));
						const __m128i xmm2 = _mm_set1_epi8(static_cast<char>(unSignatureByte));

						const __m128i xmm3 = _mm_cmpeq_epi8(xmm1, xmm2);

						unFound &= _mm_movemask_epi8(xmm3);
					}
				}
				if (unFound != 0) {
					return pData + unCycle * 16 + __bit_scan_forward(unFound);
				}
			}

			const size_t unDataBytesLeft = unSize % 16;
			if (unDataBytesLeft) {
				if (unDataBytesLeft < unSignatureLength) {
					return FindSignatureNative(pData + unSize - unDataBytesLeft - unSignatureLength, unDataBytesLeft + unSignatureLength, szSignature);
				}
				return FindSignatureNative(pData + unSize - unDataBytesLeft, unDataBytesLeft, szSignature);
			}

			return nullptr;
		}

		const void* const FindSignatureSSE2(const HMODULE hModule, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSignature) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			return FindSignatureSSE2(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureSSE2(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSignature) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSection(hModule, SectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindSignatureSSE2(pAddress, unSize, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureSSE2(const HMODULE hModule, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSectionName || !szSignature) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSectionPOGO(hModule, szSectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindSignatureSSE2(pAddress, unSize, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureSSE2A(const char* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureSSE2(hMod, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureSSE2A(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureSSE2(hMod, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureSSE2A(const char* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSectionName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureSSE2(hMod, szSectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureSSE2W(const wchar_t* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureSSE2(hMod, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureSSE2W(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureSSE2(hMod, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureSSE2W(const wchar_t* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSectionName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureSSE2(hMod, szSectionName, szSignature, unIgnoredByte);
		}

#ifdef UNICODE
		const void* const FindSignatureSSE2(const wchar_t* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureSSE2W(szModuleName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureSSE2(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureSSE2W(szModuleName, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureSSE2(const wchar_t* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureSSE2W(szModuleName, szSectionName, szSignature, unIgnoredByte);
		}
#else
		const void* const FindSignatureSSE2(const char* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureSSE2A(szModuleName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureSSE2(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureSSE2A(szModuleName, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureSSE2(const char* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureSSE2A(szModuleName, szSectionName, szSignature, unIgnoredByte);
		}
#endif

		// ----------------------------------------------------------------
		// FindSignature (AVX)
		// ----------------------------------------------------------------

		const void* const FindSignatureAVX(const void* const pAddress, const size_t unSize, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!pAddress || !unSize || !szSignature) {
				return nullptr;
			}

			const size_t unSignatureLength = strnlen_s(szSignature, 0x1000);
			if (!unSignatureLength || unSize < unSignatureLength) {
				return nullptr;
			}

			const unsigned char* const pData = reinterpret_cast<const unsigned char* const>(pAddress);
			const unsigned char* const pSignature = reinterpret_cast<const unsigned char* const>(szSignature);

			const size_t unDataBytesCycles = unSize / 32;
			for (size_t unCycle = 0; unCycle < unDataBytesCycles; ++unCycle) {
				unsigned __int32 unFound = 0xFFFFFFFFui32;
				for (size_t unSignatureIndex = 0; (unSignatureIndex < unSignatureLength) && (unFound != 0); ++unSignatureIndex) {
					const unsigned char unSignatureByte = pSignature[unSignatureIndex];
					if (unSignatureByte == unIgnoredByte) {
						continue;
					} else {
						const __m256i ymm0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(pData + unCycle * 32 + unSignatureIndex));
						const __m256i ymm1 = _mm256_set1_epi8(static_cast<char>(unSignatureByte));

						const __m128i xmm0 = _mm_cmpeq_epi8(reinterpret_cast<const __m128i*>(&ymm0)[0], reinterpret_cast<const __m128i*>(&ymm1)[0]);
						const __m128i xmm1 = _mm_cmpeq_epi8(reinterpret_cast<const __m128i*>(&ymm0)[1], reinterpret_cast<const __m128i*>(&ymm1)[1]);

						const __m256i ymm2 = _mm256_loadu2_m128i(&xmm1, &xmm0);
						reinterpret_cast<__int16*>(&unFound)[0] &= _mm_movemask_epi8(reinterpret_cast<const __m128i*>(&ymm2)[0]);
						reinterpret_cast<__int16*>(&unFound)[1] &= _mm_movemask_epi8(reinterpret_cast<const __m128i*>(&ymm2)[1]);
					}
				}
				if (unFound != 0) {
					return pData + unCycle * 32 + __bit_scan_forward(unFound);
				}
			}

			const size_t unDataBytesLeft = unSize % 32;
			if (unDataBytesLeft) {
				if (unDataBytesLeft < unSignatureLength) {
					return FindSignatureSSE2(pData + unSize - unDataBytesLeft - unSignatureLength, unDataBytesLeft + unSignatureLength, szSignature);
				}
				return FindSignatureSSE2(pData + unSize - unDataBytesLeft, unDataBytesLeft, szSignature);
			}

			return nullptr;
		}

		const void* const FindSignatureAVX(const HMODULE hModule, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSignature) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			return FindSignatureAVX(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSignature) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSection(hModule, SectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindSignatureAVX(pAddress, unSize, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX(const HMODULE hModule, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSectionName || !szSignature) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSectionPOGO(hModule, szSectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindSignatureAVX(pAddress, unSize, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVXA(const char* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX(hMod, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVXA(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX(hMod, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVXA(const char* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSectionName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX(hMod, szSectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVXW(const wchar_t* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX(hMod, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVXW(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX(hMod, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVXW(const wchar_t* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSectionName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX(hMod, szSectionName, szSignature, unIgnoredByte);
		}

#ifdef UNICODE
		const void* const FindSignatureAVX(const wchar_t* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVXW(szModuleName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVXW(szModuleName, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX(const wchar_t* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVXW(szModuleName, szSectionName, szSignature, unIgnoredByte);
		}
#else
		const void* const FindSignatureAVX(const char* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVXA(szModuleName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVXA(szModuleName, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX(const char* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVXA(szModuleName, szSectionName, szSignature, unIgnoredByte);
		}
#endif

		// ----------------------------------------------------------------
		// FindSignature (AVX2)
		// ----------------------------------------------------------------

		const void* const FindSignatureAVX2(const void* const pAddress, const size_t unSize, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!pAddress || !unSize || !szSignature) {
				return nullptr;
			}

			const size_t unSignatureLength = strnlen_s(szSignature, 0x1000);
			if (!unSignatureLength || unSize < unSignatureLength) {
				return nullptr;
			}

			const unsigned char* const pData = reinterpret_cast<const unsigned char* const>(pAddress);
			const unsigned char* const pSignature = reinterpret_cast<const unsigned char* const>(szSignature);

			const size_t unDataBytesCycles = unSize / 32;
			for (size_t unCycle = 0; unCycle < unDataBytesCycles; ++unCycle) {
				unsigned __int32 unFound = 0xFFFFFFFFui32;
				for (size_t unSignatureIndex = 0; (unSignatureIndex < unSignatureLength) && (unFound != 0); ++unSignatureIndex) {
					const unsigned char unSignatureByte = pSignature[unSignatureIndex];
					if (unSignatureByte == unIgnoredByte) {
						continue;
					} else {
						const __m256i ymm0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(pData + unCycle * 32 + unSignatureIndex));
						const __m256i ymm1 = _mm256_set1_epi8(static_cast<char>(unSignatureByte));

						const __m256i ymm3 = _mm256_cmpeq_epi8(ymm0, ymm1);

						unFound &= _mm256_movemask_epi8(ymm3);
					}
				}
				if (unFound != 0) {
					return pData + unCycle * 32 + __bit_scan_forward(unFound);
				}
			}

			const size_t unDataBytesLeft = unSize % 32;
			if (unDataBytesLeft) {
				if (unDataBytesLeft < unSignatureLength) {
					return FindSignatureAVX(pData + unSize - unDataBytesLeft - unSignatureLength, unDataBytesLeft + unSignatureLength, szSignature);
				}
				return FindSignatureAVX(pData + unSize - unDataBytesLeft, unDataBytesLeft, szSignature);
			}

			return nullptr;
		}

		const void* const FindSignatureAVX2(const HMODULE hModule, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSignature) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			return FindSignatureAVX2(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX2(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSignature) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSection(hModule, SectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindSignatureAVX2(pAddress, unSize, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX2(const HMODULE hModule, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSectionName || !szSignature) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSectionPOGO(hModule, szSectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindSignatureAVX2(pAddress, unSize, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX2A(const char* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX2(hMod, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX2A(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX2(hMod, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX2A(const char* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSectionName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX2(hMod, szSectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX2W(const wchar_t* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX2(hMod, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX2W(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX2(hMod, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX2W(const wchar_t* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSectionName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX2(hMod, szSectionName, szSignature, unIgnoredByte);
		}

#ifdef UNICODE
		const void* const FindSignatureAVX2(const wchar_t* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVX2W(szModuleName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX2(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVX2W(szModuleName, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX2(const wchar_t* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVX2W(szModuleName, szSectionName, szSignature, unIgnoredByte);
		}
#else
		const void* const FindSignatureAVX2(const char* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVX2A(szModuleName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX2(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVX2A(szModuleName, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX2(const char* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVX2A(szModuleName, szSectionName, szSignature, unIgnoredByte);
		}
#endif

		// ----------------------------------------------------------------
		// FindSignature (AVX-512) [AVX512BW]
		// ----------------------------------------------------------------

		const void* const FindSignatureAVX512(const void* const pAddress, const size_t unSize, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!pAddress || !unSize || !szSignature) {
				return nullptr;
			}

			const size_t unSignatureLength = strnlen_s(szSignature, 0x1000);
			if (!unSignatureLength || unSize < unSignatureLength) {
				return nullptr;
			}

			const unsigned char* const pData = reinterpret_cast<const unsigned char* const>(pAddress);
			const unsigned char* const pSignature = reinterpret_cast<const unsigned char* const>(szSignature);

			const size_t unDataBytesCycles = unSize / 64;
			for (size_t unCycle = 0; unCycle < unDataBytesCycles; ++unCycle) {
				unsigned __int64 unFound = 0xFFFFFFFFFFFFFFFFui64;
				for (size_t unSignatureIndex = 0; (unSignatureIndex < unSignatureLength) && (unFound != 0); ++unSignatureIndex) {
					const unsigned char unSignatureByte = pSignature[unSignatureIndex];
					if (unSignatureByte == unIgnoredByte) {
						continue;
					} else {
						const __m512i zmm0 = _mm512_loadu_si512(reinterpret_cast<const __m256i*>(pData + unCycle * 64 + unSignatureIndex));
						const __m512i zmm1 = _mm512_set1_epi8(static_cast<char>(unSignatureByte));

						unFound &= _mm512_cmpeq_epi8_mask(zmm0, zmm1);
					}
				}
				if (unFound != 0) {
					return pData + unCycle * 64 + __bit_scan_forward(unFound);
				}
			}

			const size_t unDataBytesLeft = unSize % 64;
			if (unDataBytesLeft) {
				if (unDataBytesLeft < unSignatureLength) {
					return FindSignatureAVX2(pData + unSize - unDataBytesLeft - unSignatureLength, unDataBytesLeft + unSignatureLength, szSignature);
				}
				return FindSignatureAVX2(pData + unSize - unDataBytesLeft, unDataBytesLeft, szSignature);
			}

			return nullptr;
		}

		const void* const FindSignatureAVX512(const HMODULE hModule, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSignature) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			return FindSignatureAVX512(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX512(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSignature) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSection(hModule, SectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindSignatureAVX512(pAddress, unSize, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX512(const HMODULE hModule, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSectionName || !szSignature) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSectionPOGO(hModule, szSectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindSignatureAVX512(pAddress, unSize, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX512A(const char* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX512(hMod, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX512A(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX512(hMod, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX512A(const char* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSectionName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX512(hMod, szSectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX512W(const wchar_t* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX512(hMod, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX512W(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX512(hMod, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX512W(const wchar_t* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSectionName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignatureAVX512(hMod, szSectionName, szSignature, unIgnoredByte);
		}

#ifdef UNICODE
		const void* const FindSignatureAVX512(const wchar_t* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVX512W(szModuleName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX512(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVX512W(szModuleName, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX512(const wchar_t* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVX512W(szModuleName, szSectionName, szSignature, unIgnoredByte);
		}
#else
		const void* const FindSignatureAVX512(const char* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVX512A(szModuleName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX512(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVX512A(szModuleName, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureAVX512(const char* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureAVX512A(szModuleName, szSectionName, szSignature, unIgnoredByte);
		}
#endif

		// ----------------------------------------------------------------
		// FindSignature (Auto)
		// ----------------------------------------------------------------

		static bool bOnceInitialization = false;
		static bool bProcessorFeatureSSE2 = false;
		static bool bProcessorFeatureAVX = false;
		static bool bProcessorFeatureAVX2 = false;
		static bool bProcessorFeatureAVX512BW = false;

		const void* const FindSignature(const void* const pAddress, const size_t unSize, const char* const szSignature, const unsigned char unIgnoredByte) {

			if (!bOnceInitialization) {
				bOnceInitialization = true;
				int nProcessorIDs[4];
				__cpuid(nProcessorIDs, 0x00000000);
				const int nIDs = nProcessorIDs[0];
				if (nIDs >= 1) {
					__cpuid(nProcessorIDs, 0x00000001);
					bProcessorFeatureSSE2 = (nProcessorIDs[3] & (1 << 26)) != 0;
					bProcessorFeatureAVX = (nProcessorIDs[2] & (1 << 28)) != 0;
					if (nIDs >= 7) {
						__cpuid(nProcessorIDs, 0x00000007);
						bProcessorFeatureAVX2 = (nProcessorIDs[1] & (1 << 5)) != 0;
						bProcessorFeatureAVX512BW = (nProcessorIDs[1] & (1 << 30)) != 0;
					}
				}
			}

			if (bProcessorFeatureAVX512BW) {
				return FindSignatureAVX512(pAddress, unSize, szSignature, unIgnoredByte);
			} else if (bProcessorFeatureAVX2) {
				return FindSignatureAVX2(pAddress, unSize, szSignature, unIgnoredByte);
			} else if (bProcessorFeatureAVX) {
				return FindSignatureAVX(pAddress, unSize, szSignature, unIgnoredByte);
			} else if (bProcessorFeatureSSE2) {
				return FindSignatureSSE2(pAddress, unSize, szSignature, unIgnoredByte);
			} else {
				return FindSignatureNative(pAddress, unSize, szSignature, unIgnoredByte);
			}
		}

		const void* const FindSignature(const HMODULE hModule, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSignature) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			return FindSignature(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, szSignature, unIgnoredByte);
		}

		const void* const FindSignature(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSignature) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSection(hModule, SectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindSignature(pAddress, unSize, szSignature, unIgnoredByte);
		}

		const void* const FindSignature(const HMODULE hModule, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!hModule || !szSectionName || !szSignature) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSectionPOGO(hModule, szSectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindSignature(pAddress, unSize, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureA(const char* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignature(hMod, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureA(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignature(hMod, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureA(const char* const szModuleName, const char* const szSectionName,  const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSectionName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignature(hMod, szSectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureW(const wchar_t* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignature(hMod, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureW(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignature(hMod, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignatureW(const wchar_t* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			if (!szModuleName || !szSectionName || !szSignature) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindSignature(hMod, szSectionName, szSignature, unIgnoredByte);
		}

#ifdef UNICODE
		const void* const FindSignature(const wchar_t* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureW(szModuleName, szSignature, unIgnoredByte);
		}

		const void* const FindSignature(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureW(szModuleName, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignature(const wchar_t* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureW(szModuleName, szSectionName, szSignature, unIgnoredByte);
		}
#else
		const void* const FindSignature(const char* const szModuleName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureA(szModuleName, szSignature, unIgnoredByte);
		}

		const void* const FindSignature(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureA(szModuleName, SectionName, szSignature, unIgnoredByte);
		}

		const void* const FindSignature(const char* const szModuleName, const char* const szSectionName, const char* const szSignature, const unsigned char unIgnoredByte) {
			return FindSignatureA(szModuleName, szSectionName, szSignature, unIgnoredByte);
		}
#endif

		// ----------------------------------------------------------------
		// FindData (Native)
		// ----------------------------------------------------------------

		const void* const FindDataNative(const void* const pAddress, const size_t unSize, const unsigned char* const pData, const size_t unDataSize) {
			if (!pAddress || !unSize || !pData || !unDataSize || (unSize < unDataSize)) {
				return nullptr;
			}

			const unsigned char* const pSourceData = reinterpret_cast<const unsigned char* const>(pAddress);

			for (size_t unIndex = 0; unIndex < unSize; ++unIndex) {
				size_t unDataIndex = 0;
				for (; unDataIndex < unDataSize; ++unDataIndex) {
					if (pSourceData[unIndex + unDataIndex] != pData[unDataIndex]) {
						break;
					}
				}
				if (unDataIndex == unDataSize) {
					return pSourceData + unIndex;
				}
			}

			return nullptr;
		}

		const void* const FindDataNative(const HMODULE hModule, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			return FindDataNative(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, pData, unDataSize);
		}

		const void* const FindDataNative(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSection(hModule, SectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindDataNative(pAddress, unSize, pData, unDataSize);
		}

		const void* const FindDataNative(const HMODULE hModule, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSectionPOGO(hModule, szSectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindDataNative(pAddress, unSize, pData, unDataSize);
		}

		const void* const FindDataNativeA(const char* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataNative(hMod, pData, unDataSize);
		}

		const void* const FindDataNativeA(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataNative(hMod, SectionName, pData, unDataSize);
		}
		
		const void* const FindDataNativeA(const char* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataNative(hMod, szSectionName, pData, unDataSize);
		}

		const void* const FindDataNativeW(const wchar_t* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataNative(hMod, pData, unDataSize);
		}

		const void* const FindDataNativeW(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataNative(hMod, SectionName, pData, unDataSize);
		}

		const void* const FindDataNativeW(const wchar_t* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataNative(hMod, szSectionName, pData, unDataSize);
		}

#ifdef UNICODE
		const void* const FindDataNative(const wchar_t* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataNativeW(szModuleName, pData, unDataSize);
		}

		const void* const FindDataNative(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataNativeW(szModuleName, SectionName, pData, unDataSize);
		}

		const void* const FindDataNative(const wchar_t* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataNativeW(szModuleName, szSectionName, pData, unDataSize);
		}
#else
		const void* const FindDataNative(const char* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataNativeA(szModuleName, pData, unDataSize);
		}

		const void* const FindDataNative(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataNativeA(szModuleName, SectionName, pData, unDataSize);
		}

		const void* const FindDataNative(const char* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataNativeA(szModuleName, szSectionName, pData, unDataSize);
		}
#endif

		// ----------------------------------------------------------------
		// FindData (SSE2)
		// ----------------------------------------------------------------

		const void* const FindDataSSE2(const void* const pAddress, const size_t unSize, const unsigned char* const pData, const size_t unDataSize) {
			if (!pAddress || !unSize || !pData || !unDataSize || (unSize < unDataSize)) {
				return nullptr;
			}

			const unsigned char* const pSourceData = reinterpret_cast<const unsigned char* const>(pAddress);

			const size_t unDataBytesCycles = static_cast<size_t>(floor(static_cast<double>(unSize) / 16.0));
			for (size_t unCycle = 0; unCycle < unDataBytesCycles; ++unCycle) {
				unsigned __int16 unFound = 0xFFFFui16;
				for (size_t unDataIndex = 0; (unDataIndex < unDataSize) && (unFound != 0); ++unDataIndex) {
					const __m128i xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSourceData + unCycle * 16 + unDataIndex));
					const __m128i xmm2 = _mm_set1_epi8(static_cast<char>(pData[unDataIndex]));

					const __m128i xmm3 = _mm_cmpeq_epi8(xmm1, xmm2);

					unFound &= _mm_movemask_epi8(xmm3);
				}
				if (unFound != 0) {
					return pSourceData + unCycle * 16 + __bit_scan_forward(unFound);
				}
			}

			const size_t unDataBytesLeft = unSize - unDataBytesCycles * 16;
			if (unDataBytesLeft) {
				if (unDataBytesLeft < unDataSize) {
					return FindDataNative(pSourceData + unSize - unDataBytesLeft - unDataSize, unDataBytesLeft + unDataSize, pData, unDataSize);
				}
				return FindDataNative(pSourceData + unSize - unDataBytesLeft, unDataBytesLeft, pData, unDataSize);
			}

			return nullptr;
		}

		const void* const FindDataSSE2(const HMODULE hModule, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			return FindDataSSE2(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, pData, unDataSize);
		}

		const void* const FindDataSSE2(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSection(hModule, SectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindDataSSE2(pAddress, unSize, pData, unDataSize);
		}

		const void* const FindDataSSE2(const HMODULE hModule, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSectionPOGO(hModule, szSectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindDataSSE2(pAddress, unSize, pData, unDataSize);
		}

		const void* const FindDataSSE2A(const char* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataSSE2(hMod, pData, unDataSize);
		}

		const void* const FindDataSSE2A(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataSSE2(hMod, SectionName, pData, unDataSize);
		}

		const void* const FindDataSSE2A(const char* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataSSE2(hMod, szSectionName, pData, unDataSize);
		}

		const void* const FindDataSSE2W(const wchar_t* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataSSE2(hMod, pData, unDataSize);
		}

		const void* const FindDataSSE2W(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataSSE2(hMod, SectionName, pData, unDataSize);
		}

		const void* const FindDataSSE2W(const wchar_t* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataSSE2(hMod, szSectionName, pData, unDataSize);
		}

#ifdef UNICODE
		const void* const FindDataSSE2(const wchar_t* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataSSE2W(szModuleName, pData, unDataSize);
		}

		const void* const FindDataSSE2(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataSSE2W(szModuleName, SectionName, pData, unDataSize);
		}

		const void* const FindDataSSE2(const wchar_t* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataSSE2W(szModuleName, szSectionName, pData, unDataSize);
		}
#else
		const void* const FindDataSSE2(const char* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataSSE2A(szModuleName, pData, unDataSize);
		}

		const void* const FindDataSSE2(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataSSE2A(szModuleName, SectionName, pData, unDataSize);
		}

		const void* const FindDataSSE2(const char* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataSSE2A(szModuleName, szSectionName, pData, unDataSize);
		}
#endif

		// ----------------------------------------------------------------
		// FindData (AVX)
		// ----------------------------------------------------------------

		const void* const FindDataAVX(const void* const pAddress, const size_t unSize, const unsigned char* const pData, const size_t unDataSize) {
			if (!pAddress || !unSize || !pData || !unDataSize || (unSize < unDataSize)) {
				return nullptr;
			}

			const unsigned char* const pSourceData = reinterpret_cast<const unsigned char* const>(pAddress);

			const size_t unDataBytesCycles = static_cast<size_t>(floor(static_cast<double>(unSize) / 32.0));
			for (size_t unCycle = 0; unCycle < unDataBytesCycles; ++unCycle) {
				unsigned __int32 unFound = 0xFFFFFFFFui32;
				for (size_t unDataIndex = 0; (unDataIndex < unDataSize) && (unFound != 0); ++unDataIndex) {
					const __m256i ymm0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(pSourceData + unCycle * 32 + unDataIndex));
					const __m256i ymm1 = _mm256_set1_epi8(static_cast<char>(pData[unDataIndex]));

					const __m128i xmm0 = _mm_cmpeq_epi8(reinterpret_cast<const __m128i*>(&ymm0)[0], reinterpret_cast<const __m128i*>(&ymm1)[0]);
					const __m128i xmm1 = _mm_cmpeq_epi8(reinterpret_cast<const __m128i*>(&ymm0)[1], reinterpret_cast<const __m128i*>(&ymm1)[1]);

					const __m256i ymm2 = _mm256_loadu2_m128i(&xmm1, &xmm0);
					reinterpret_cast<__int16*>(&unFound)[0] &= _mm_movemask_epi8(reinterpret_cast<const __m128i*>(&ymm2)[0]);
					reinterpret_cast<__int16*>(&unFound)[1] &= _mm_movemask_epi8(reinterpret_cast<const __m128i*>(&ymm2)[1]);
				}
				if (unFound != 0) {
					return pSourceData + unCycle * 32 + __bit_scan_forward(unFound);
				}
			}

			const size_t unDataBytesLeft = unSize - unDataBytesCycles * 32;
			if (unDataBytesLeft) {
				if (unDataBytesLeft < unDataSize) {
					return FindDataSSE2(pSourceData + unSize - unDataBytesLeft - unDataSize, unDataBytesLeft + unDataSize, pData, unDataSize);
				}
				return FindDataSSE2(pSourceData + unSize - unDataBytesLeft, unDataBytesLeft, pData, unDataSize);
			}

			return nullptr;
		}

		const void* const FindDataAVX(const HMODULE hModule, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			return FindDataAVX(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, pData, unDataSize);
		}

		const void* const FindDataAVX(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSection(hModule, SectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindDataAVX(pAddress, unSize, pData, unDataSize);
		}

		const void* const FindDataAVX(const HMODULE hModule, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSectionPOGO(hModule, szSectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindDataAVX(pAddress, unSize, pData, unDataSize);
		}

		const void* const FindDataAVXA(const char* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX(hMod, pData, unDataSize);
		}

		const void* const FindDataAVXA(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX(hMod, SectionName, pData, unDataSize);
		}

		const void* const FindDataAVXA(const char* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX(hMod, szSectionName, pData, unDataSize);
		}

		const void* const FindDataAVXW(const wchar_t* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX(hMod, pData, unDataSize);
		}

		const void* const FindDataAVXW(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX(hMod, SectionName, pData, unDataSize);
		}

		const void* const FindDataAVXW(const wchar_t* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX(hMod, szSectionName, pData, unDataSize);
		}

#ifdef UNICODE
		const void* const FindDataAVX(const wchar_t* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVXW(szModuleName, pData, unDataSize);
		}

		const void* const FindDataAVX(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVXW(szModuleName, SectionName, pData, unDataSize);
		}

		const void* const FindDataAVX(const wchar_t* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVXW(szModuleName, szSectionName, pData, unDataSize);
		}
#else
		const void* const FindDataAVX(const char* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVXA(szModuleName, pData, unDataSize);
		}

		const void* const FindDataAVX(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVXA(szModuleName, SectionName, pData, unDataSize);
		}

		const void* const FindDataAVX(const char* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVXA(szModuleName, szSectionName, pData, unDataSize);
		}
#endif

		// ----------------------------------------------------------------
		// FindData (AVX2)
		// ----------------------------------------------------------------

		const void* const FindDataAVX2(const void* const pAddress, const size_t unSize, const unsigned char* const pData, const size_t unDataSize) {
			if (!pAddress || !unSize || !pData || !unDataSize || (unSize < unDataSize)) {
				return nullptr;
			}

			const unsigned char* const pSourceData = reinterpret_cast<const unsigned char* const>(pAddress);

			const size_t unDataBytesCycles = static_cast<size_t>(floor(static_cast<double>(unSize) / 32.0));
			for (size_t unCycle = 0; unCycle < unDataBytesCycles; ++unCycle) {
				unsigned __int32 unFound = 0xFFFFFFFFui32;
				for (size_t unDataIndex = 0; (unDataIndex < unDataSize) && (unFound != 0); ++unDataIndex) {
					const __m256i ymm0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(pSourceData + unCycle * 32 + unDataIndex));
					const __m256i ymm1 = _mm256_set1_epi8(static_cast<char>(pData[unDataIndex]));

					const __m256i ymm3 = _mm256_cmpeq_epi8(ymm0, ymm1);

					unFound &= _mm256_movemask_epi8(ymm3);
				}
				if (unFound != 0) {
					return pSourceData + unCycle * 32 + __bit_scan_forward(unFound);
				}
			}

			const size_t unDataBytesLeft = unSize - unDataBytesCycles * 32;
			if (unDataBytesLeft) {
				if (unDataBytesLeft < unDataSize) {
					return FindDataAVX(pSourceData + unSize - unDataBytesLeft - unDataSize, unDataBytesLeft + unDataSize, pData, unDataSize);
				}
				return FindDataAVX(pSourceData + unSize - unDataBytesLeft, unDataBytesLeft, pData, unDataSize);
			}

			return nullptr;
		}

		const void* const FindDataAVX2(const HMODULE hModule, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			return FindDataAVX2(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, pData, unDataSize);
		}

		const void* const FindDataAVX2(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSection(hModule, SectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindDataAVX2(pAddress, unSize, pData, unDataSize);
		}

		const void* const FindDataAVX2(const HMODULE hModule, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSectionPOGO(hModule, szSectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindDataAVX2(pAddress, unSize, pData, unDataSize);
		}

		const void* const FindDataAVX2A(const char* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX2(hMod, pData, unDataSize);
		}

		const void* const FindDataAVX2A(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX2(hMod, SectionName, pData, unDataSize);
		}

		const void* const FindDataAVX2A(const char* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX2(hMod, szSectionName, pData, unDataSize);
		}

		const void* const FindDataAVX2W(const wchar_t* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX2(hMod, pData, unDataSize);
		}

		const void* const FindDataAVX2W(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX2(hMod, SectionName, pData, unDataSize);
		}

		const void* const FindDataAVX2W(const wchar_t* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX2(hMod, szSectionName, pData, unDataSize);
		}

#ifdef UNICODE
		const void* const FindDataAVX2(const wchar_t* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVX2W(szModuleName, pData, unDataSize);
		}

		const void* const FindDataAVX2(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVX2W(szModuleName, SectionName, pData, unDataSize);
		}

		const void* const FindDataAVX2(const wchar_t* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVX2W(szModuleName, szSectionName, pData, unDataSize);
		}
#else
		const void* const FindDataAVX2(const char* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVX2A(szModuleName, pData, unDataSize);
		}

		const void* const FindDataAVX2(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVX2A(szModuleName, SectionName, pData, unDataSize);
		}

		const void* const FindDataAVX2(const char* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVX2A(szModuleName, szSectionName, pData, unDataSize);
		}
#endif

		// ----------------------------------------------------------------
		// FindData (AVX-512) [AVX512BW]
		// ----------------------------------------------------------------

		const void* const FindDataAVX512(const void* const pAddress, const size_t unSize, const unsigned char* const pData, const size_t unDataSize) {
			if (!pAddress || !unSize || !pData || !unDataSize || (unSize < unDataSize)) {
				return nullptr;
			}

			const unsigned char* const pSourceData = reinterpret_cast<const unsigned char* const>(pAddress);

			const size_t unDataBytesCycles = static_cast<size_t>(floor(static_cast<double>(unSize) / 64.0));
			for (size_t unCycle = 0; unCycle < unDataBytesCycles; ++unCycle) {
				unsigned __int64 unFound = 0xFFFFFFFFFFFFFFFFui64;
				for (size_t unDataIndex = 0; (unDataIndex < unDataSize) && (unFound != 0); ++unDataIndex) {
					const __m512i zmm0 = _mm512_loadu_si512(reinterpret_cast<const __m256i*>(pSourceData + unCycle * 64 + unDataIndex));
					const __m512i zmm1 = _mm512_set1_epi8(static_cast<char>(pData[unDataIndex]));

					unFound &= _mm512_cmpeq_epi8_mask(zmm0, zmm1);
				}
				if (unFound != 0) {
					return pSourceData + unCycle * 64 + __bit_scan_forward(unFound);
				}
			}

			const size_t unDataBytesLeft = unSize - unDataBytesCycles * 64;
			if (unDataBytesLeft) {
				if (unDataBytesLeft < unDataSize) {
					return FindDataAVX2(pSourceData + unSize - unDataBytesLeft - unDataSize, unDataBytesLeft + unDataSize, pData, unDataSize);
				}
				return FindDataAVX2(pSourceData + unSize - unDataBytesLeft, unDataBytesLeft, pData, unDataSize);
			}

			return nullptr;
		}

		const void* const FindDataAVX512(const HMODULE hModule, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			return FindDataAVX512(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, pData, unDataSize);
		}

		const void* const FindDataAVX512(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSection(hModule, SectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindDataAVX512(pAddress, unSize, pData, unDataSize);
		}

		const void* const FindDataAVX512(const HMODULE hModule, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSectionPOGO(hModule, szSectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindDataAVX512(pAddress, unSize, pData, unDataSize);
		}

		const void* const FindDataAVX512A(const char* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX512(hMod, pData, unDataSize);
		}

		const void* const FindDataAVX512A(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX512(hMod, pData, unDataSize);
		}

		const void* const FindDataAVX512A(const char* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX512(hMod, pData, unDataSize);
		}

		const void* const FindDataAVX512W(const wchar_t* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX512(hMod, pData, unDataSize);
		}

		const void* const FindDataAVX512W(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX512(hMod, SectionName, pData, unDataSize);
		}

		const void* const FindDataAVX512W(const wchar_t* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindDataAVX512(hMod, szSectionName, pData, unDataSize);
		}

#ifdef UNICODE
		const void* const FindDataAVX512(const wchar_t* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVX512W(szModuleName, pData, unDataSize);
		}

		const void* const FindDataAVX512(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVX512W(szModuleName, SectionName, pData, unDataSize);
		}

		const void* const FindDataAVX512(const wchar_t* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVX512W(szModuleName, szSectionName, pData, unDataSize);
		}
#else
		const void* const FindDataAVX512(const char* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVX512A(szModuleName, pData, unDataSize);
		}

		const void* const FindDataAVX512(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVX512A(szModuleName, SectionName, pData, unDataSize);
		}

		const void* const FindDataAVX512(const char* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataAVX512A(szModuleName, szSectionName, pData, unDataSize);
		}
#endif

		// ----------------------------------------------------------------
		// FindData (Auto)
		// ----------------------------------------------------------------

		const void* const FindData(const void* const pAddress, const size_t unSize, const unsigned char* const pData, const size_t unDataSize) {

			if (!bOnceInitialization) {
				bOnceInitialization = true;
				int nProcessorIDs[4];
				__cpuid(nProcessorIDs, 0x00000000);
				const int nIDs = nProcessorIDs[0];
				if (nIDs >= 1) {
					__cpuid(nProcessorIDs, 0x00000001);
					bProcessorFeatureSSE2 = (nProcessorIDs[3] & (1 << 26)) != 0;
					bProcessorFeatureAVX = (nProcessorIDs[2] & (1 << 28)) != 0;
					if (nIDs >= 7) {
						__cpuid(nProcessorIDs, 0x00000007);
						bProcessorFeatureAVX2 = (nProcessorIDs[1] & (1 << 5)) != 0;
						bProcessorFeatureAVX512BW = (nProcessorIDs[1] & (1 << 30)) != 0;
					}
				}
			}

			if (bProcessorFeatureAVX512BW) {
				return FindDataAVX512(pAddress, unSize, pData, unDataSize);
			} else if (bProcessorFeatureAVX2) {
				return FindDataAVX2(pAddress, unSize, pData, unDataSize);
			} else if (bProcessorFeatureAVX) {
				return FindDataAVX(pAddress, unSize, pData, unDataSize);
			} else if (bProcessorFeatureSSE2) {
				return FindDataSSE2(pAddress, unSize, pData, unDataSize);
			} else {
				return FindDataNative(pAddress, unSize, pData, unDataSize);
			}
		}

		const void* const FindData(const HMODULE hModule, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			return FindData(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, pData, unDataSize);
		}

		const void* const FindData(const HMODULE hModule, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSection(hModule, SectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindData(pAddress, unSize, pData, unDataSize);
		}

		const void* const FindData(const HMODULE hModule, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!hModule || !pData || !unDataSize) {
				return nullptr;
			}

			void* pAddress = nullptr;
			size_t unSize = 0;
			if (!FindSectionPOGO(hModule, szSectionName, &pAddress, &unSize)) {
				return nullptr;
			}

			return FindData(pAddress, unSize, pData, unDataSize);
		}

		const void* const FindDataA(const char* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindData(hMod, pData, unDataSize);
		}

		const void* const FindDataA(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindData(hMod, SectionName, pData, unDataSize);
		}

		const void* const FindDataA(const char* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindData(hMod, szSectionName, pData, unDataSize);
		}

		const void* const FindDataW(const wchar_t* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindData(hMod, pData, unDataSize);
		}

		const void* const FindDataW(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindData(hMod, SectionName, pData, unDataSize);
		}

		const void* const FindDataW(const wchar_t* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			if (!szModuleName || !szSectionName || !pData || !unDataSize) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindData(hMod, szSectionName, pData, unDataSize);
		}

#ifdef UNICODE
		const void* const FindData(const wchar_t* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataW(szModuleName, pData, unDataSize);
		}

		const void* const FindData(const wchar_t* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataW(szModuleName, SectionName, pData, unDataSize);
		}

		const void* const FindData(const wchar_t* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataW(szModuleName, szSectionName, pData, unDataSize);
		}
#else
		const void* const FindData(const char* const szModuleName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataA(szModuleName, pData, unDataSize);
		}

		const void* const FindData(const char* const szModuleName, const std::array<const unsigned char, 8>& SectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataA(szModuleName, SectionName, pData, unDataSize);
		}

		const void* const FindData(const char* const szModuleName, const char* const szSectionName, const unsigned char* const pData, const size_t unDataSize) {
			return FindDataA(szModuleName, szSectionName, pData, unDataSize);
		}
#endif

		// ----------------------------------------------------------------
		// RTTI
		// ----------------------------------------------------------------

#pragma pack(push, 1)
		typedef struct _PMD {
			int m_nMDisp;
			int m_nPDisp;
			int m_nVDisp;
		} PMD, *PPMD;

		typedef struct _TYPE_DESCRIPTOR {
			void* m_pVFTable;
			void* m_pSpare;
			char m_szName[1];
		} TYPE_DESCRIPTOR, *PTYPE_DESCRIPTOR;

		typedef struct _RTTI_BASE_CLASS_DESCRIPTOR {
#ifdef _M_X64
			unsigned int m_unTypeDescriptor;
#elif _M_IX86
			PTYPE_DESCRIPTOR m_pTypeDescriptor;
#endif
			unsigned int m_unNumberOfContainedBases;
			PMD m_Where;
			unsigned int m_unAttributes;
		} RTTI_BASE_CLASS_DESCRIPTOR, *PRTTI_BASE_CLASS_DESCRIPTOR;

		typedef struct _RTTI_BASE_CLASS_ARRAY {
#ifdef _M_X64
			unsigned int m_unBaseClassDescriptors;
#elif _M_IX86
			PRTTI_BASE_CLASS_DESCRIPTOR m_pBaseClassDescriptors[1];
#endif
		} RTTI_BASE_CLASS_ARRAY, *PRTTI_BASE_CLASS_ARRAY;

		typedef struct _RTTI_CLASS_HIERARCHY_DESCRIPTOR {
			unsigned int m_unSignature;
			unsigned int m_unAttributes;
			unsigned int m_unNumberOfBaseClasses;
#ifdef _M_X64
			unsigned int m_unBaseClassArray;
#elif _M_IX86
			PRTTI_BASE_CLASS_ARRAY m_pBaseClassArray;
#endif
		} RTTI_CLASS_HIERARCHY_DESCRIPTOR, *PRTTI_CLASS_HIERARCHY_DESCRIPTOR;

		typedef struct _RTTI_COMPLETE_OBJECT_LOCATOR {
			unsigned int m_unSignature;
			unsigned int m_unOffset;
			unsigned int m_unConstructorOffset;
#ifdef _M_X64
			unsigned int m_unTypeDescriptor;
			unsigned int m_unClassHierarchyDescriptor;
#elif _M_IX86
			PTYPE_DESCRIPTOR m_pTypeDescriptor;
			PRTTI_CLASS_HIERARCHY_DESCRIPTOR m_pClassHierarchyDescriptor;
#endif
		} RTTI_COMPLETE_OBJECT_LOCATOR, *PRTTI_COMPLETE_OBJECT_LOCATOR;
#pragma pack(pop)

		// ----------------------------------------------------------------
		// FindRTTI
		// ----------------------------------------------------------------

		static const void* const FindRTTI(const void* const pBaseAddress, const void* const pAddress, const size_t unSize, const char* const szRTTI) {
			if (!pBaseAddress || !unSize || !szRTTI) {
				return nullptr;
			}

			const size_t unRTTILength = strnlen_s(szRTTI, 0x1000);
			if (!unRTTILength || (unSize <= unRTTILength)) {
				return nullptr;
			}

			void* pReference = const_cast<void*>(pAddress);
			void* pEndAddress = reinterpret_cast<char*>(const_cast<void*>(pAddress)) + unSize;
			while (pReference && (pReference < pEndAddress)) {
				pReference = const_cast<void*>(FindData(pReference, reinterpret_cast<size_t>(pEndAddress) - reinterpret_cast<size_t>(pReference), reinterpret_cast<const unsigned char* const>(szRTTI), unRTTILength));
				if (!pReference) {
					break;
				}

				const PTYPE_DESCRIPTOR pTypeDescriptor = reinterpret_cast<PTYPE_DESCRIPTOR>(reinterpret_cast<char*>(pReference) - sizeof(void*) * 2);
				if ((pTypeDescriptor->m_pVFTable < pBaseAddress) || (pTypeDescriptor->m_pVFTable >= pEndAddress)) {
					pReference = reinterpret_cast<void*>(reinterpret_cast<char*>(pReference) + 1);
					continue;
				}
				if (pTypeDescriptor->m_pSpare) {
					pReference = reinterpret_cast<void*>(reinterpret_cast<char*>(pReference) + 1);
					continue;
				}

				void* pTypeDescriptorReference = const_cast<void*>(pAddress);
				while (pTypeDescriptorReference && (pTypeDescriptorReference < pEndAddress)) {
#ifdef _M_X64
					const size_t unTypeDescriptorOffsetTemp = reinterpret_cast<size_t>(pTypeDescriptor) - reinterpret_cast<size_t>(pBaseAddress);
					const unsigned int unTypeDescriptorOffset = (*(reinterpret_cast<const unsigned int*>(&unTypeDescriptorOffsetTemp)));
					pTypeDescriptorReference = const_cast<void*>(FindData(pTypeDescriptorReference, reinterpret_cast<size_t>(pEndAddress) - reinterpret_cast<size_t>(pTypeDescriptorReference), reinterpret_cast<const unsigned char* const>(&unTypeDescriptorOffset), sizeof(int)));
					if (!pTypeDescriptorReference) {
						break;
					}
#elif _M_IX86
					pTypeDescriptorReference = const_cast<void*>(FindData(pTypeDescriptorReference, reinterpret_cast<size_t>(pEndAddress) - reinterpret_cast<size_t>(pTypeDescriptorReference), reinterpret_cast<const unsigned char* const>(&pTypeDescriptor), sizeof(int)));
					if (!pTypeDescriptorReference) {
						break;
					}
#endif

					const PRTTI_COMPLETE_OBJECT_LOCATOR pCompleteObjectLocation = reinterpret_cast<PRTTI_COMPLETE_OBJECT_LOCATOR>(reinterpret_cast<char*>(pTypeDescriptorReference) - sizeof(int) * 3);
#ifdef _M_X64
					const PRTTI_CLASS_HIERARCHY_DESCRIPTOR pClassHierarchyDescriptor = reinterpret_cast<PRTTI_CLASS_HIERARCHY_DESCRIPTOR>(reinterpret_cast<size_t>(pBaseAddress) + pCompleteObjectLocation->m_unClassHierarchyDescriptor);
#elif _M_IX86
					const PRTTI_CLASS_HIERARCHY_DESCRIPTOR pClassHierarchyDescriptor = pCompleteObjectLocation->m_pClassHierarchyDescriptor;
#endif
					if ((pClassHierarchyDescriptor < pBaseAddress) || (pClassHierarchyDescriptor >= pEndAddress)) {
						pTypeDescriptorReference = reinterpret_cast<void*>(reinterpret_cast<char*>(pTypeDescriptorReference) + 1);
						continue;
					}

#ifdef _M_X64
					const PRTTI_BASE_CLASS_ARRAY pBaseClassArray = reinterpret_cast<PRTTI_BASE_CLASS_ARRAY>(reinterpret_cast<size_t>(pBaseAddress) + pClassHierarchyDescriptor->m_unBaseClassArray);
#elif _M_IX86
					const PRTTI_BASE_CLASS_ARRAY pBaseClassArray = pClassHierarchyDescriptor->m_pBaseClassArray;
#endif
					if ((pBaseClassArray < pBaseAddress) || (pBaseClassArray >= pEndAddress)) {
						pTypeDescriptorReference = reinterpret_cast<void*>(reinterpret_cast<char*>(pTypeDescriptorReference) + 1);
						continue;
					}

#ifdef _M_X64
					const PRTTI_BASE_CLASS_DESCRIPTOR pBaseClassDescriptors = reinterpret_cast<PRTTI_BASE_CLASS_DESCRIPTOR>(reinterpret_cast<size_t>(pBaseAddress) + pBaseClassArray->m_unBaseClassDescriptors);
#elif _M_IX86
					const PRTTI_BASE_CLASS_DESCRIPTOR pBaseClassDescriptors = pBaseClassArray->m_pBaseClassDescriptors[0];
#endif
					if ((pBaseClassDescriptors < pBaseAddress) || (pBaseClassDescriptors >= pEndAddress)) {
						pTypeDescriptorReference = reinterpret_cast<void*>(reinterpret_cast<char*>(pTypeDescriptorReference) + 1);
						continue;
					}

					for (size_t i = 0; i < pClassHierarchyDescriptor->m_unNumberOfBaseClasses; ++i) {
						const PRTTI_BASE_CLASS_DESCRIPTOR pBaseClassDescriptor = (&pBaseClassDescriptors)[i];
						if (!pBaseClassDescriptor) {
							continue;
						}
#ifdef _M_X64
						if (reinterpret_cast<void*>(reinterpret_cast<size_t>(pBaseAddress) + pBaseClassDescriptor->m_unTypeDescriptor) == pTypeDescriptor) {
#elif _M_IX86
						if (pBaseClassDescriptor->m_pTypeDescriptor == pTypeDescriptor) {
#endif
							const void* const pCompleteObject = FindData(pAddress, unSize, reinterpret_cast<const unsigned char* const>(&pCompleteObjectLocation), sizeof(void*));
							if (!pCompleteObject) {
								return nullptr;
							}
							return reinterpret_cast<const void* const>(reinterpret_cast<const unsigned char* const>(pCompleteObject) + sizeof(void*));
						}
					}

					pTypeDescriptorReference = reinterpret_cast<void*>(reinterpret_cast<char*>(pTypeDescriptorReference) + 1);
				}

				pReference = reinterpret_cast<void*>(reinterpret_cast<char*>(pReference) + 1);
			}

			return nullptr;
		}

		const void* const FindRTTI(const void* const pBaseAddress, const size_t unSize, const char* const szRTTI) {
			return FindRTTI(pBaseAddress, pBaseAddress, unSize, szRTTI);
		}

		const void* const FindRTTI(const HMODULE hModule, const char* const szRTTI) {
			if (!hModule || !szRTTI) {
				return nullptr;
			}

			const PIMAGE_DOS_HEADER pDH = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
			const PIMAGE_NT_HEADERS pNTHs = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(hModule) + pDH->e_lfanew);
			const PIMAGE_OPTIONAL_HEADER pOH = &(pNTHs->OptionalHeader);

			void* pFirstSection = nullptr;
			size_t unFirstSectionSize = 0;
			void* pSecondSection = nullptr;
			size_t unSecondSectionSize = 0;

			if (FindSectionPOGO(hModule, ".rdata", &pFirstSection, &unFirstSectionSize) && FindSectionPOGO(hModule, ".data$rs", &pSecondSection, &unSecondSectionSize)) {
				return FindRTTI(reinterpret_cast<void*>(hModule), pFirstSection, reinterpret_cast<size_t>(pSecondSection) - reinterpret_cast<size_t>(pFirstSection) + unSecondSectionSize, szRTTI);
			}

			if (FindSection(hModule, { '.', 'r', 'd', 'a', 't', 'a', 0, 0 }, &pFirstSection, &unFirstSectionSize) && FindSection(hModule, { '.', 'd', 'a', 't', 'a',   0, 0, 0 }, &pSecondSection, &unSecondSectionSize)) {
				return FindRTTI(reinterpret_cast<void*>(hModule), pFirstSection, reinterpret_cast<size_t>(pSecondSection) - reinterpret_cast<size_t>(pFirstSection) + unSecondSectionSize, szRTTI);
			}

			return FindRTTI(reinterpret_cast<void*>(hModule), static_cast<size_t>(pOH->SizeOfImage) - 1, szRTTI);
		}

		const void* const FindRTTIA(const char* const szModuleName, const char* const szRTTI) {
			if (!szModuleName || szRTTI) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleA(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindRTTI(hMod, szRTTI);
		}

		const void* const FindRTTIW(const wchar_t* const szModuleName, const char* const szRTTI) {
			if (!szModuleName || szRTTI) {
				return nullptr;
			}

			const HMODULE hMod = GetModuleHandleW(szModuleName);
			if (!hMod) {
				return nullptr;
			}

			return FindRTTI(hMod, szRTTI);
		}

#ifdef UNICODE
		const void* const FindRTTI(const wchar_t* const szModuleName, const char* const szRTTI) {
			return FindRTTIW(szModuleName, szRTTI);
		}
#else
		const void* const FindRTTI(const char* const szModuleName, const char* const szRTTI) {
			return FindRTTIA(szModuleName, szRTTI);
		}
#endif
	}

	// ----------------------------------------------------------------
	// Memory
	// ----------------------------------------------------------------

	namespace Memory {

		// ----------------------------------------------------------------
		// Server
		// ----------------------------------------------------------------

		Server::Server(const size_t unMemorySize, bool bIsGlobal) {
			memset(m_szSessionName, 0, sizeof(m_szSessionName));
			m_hMap = nullptr;
			m_pAddress = nullptr;

			if (!unMemorySize) {
				return;
			}

			const DWORD unPID = GetCurrentProcessId();
			const DWORD unTID = GetCurrentThreadId();
			const DWORD64 unCycle = __rdtsc();
			_stprintf_s(m_szSessionName, _T("GLOBAL:%08X:%08X:%08X%08X"), 0xFFFFFFFFi32 - unPID, 0xFFFFFFFFi32 - unTID, static_cast<DWORD>(unCycle & 0xFFFFFFFFi32), static_cast<DWORD>((unCycle >> 32) & 0xFFFFFFFFi32));

			TCHAR szMap[64];
			memset(szMap, 0, sizeof(szMap));
			if (bIsGlobal) {
				_stprintf_s(szMap, _T("Global\\%s"), m_szSessionName);
			} else {
				_stprintf_s(szMap, _T("Local\\%s"), m_szSessionName);
			}

#ifdef _M_X64
			m_hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, static_cast<DWORD>((unMemorySize >> 32) & 0xFFFFFFFFi32), static_cast<DWORD>(unMemorySize & 0xFFFFFFFFi32), szMap);
#elif _M_IX86
			m_hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, NULL, static_cast<DWORD>(unMemorySize & 0xFFFFFFFFi32), szMap);
#endif
			if (m_hMap && (m_hMap != INVALID_HANDLE_VALUE)) {
				m_pAddress = MapViewOfFile(m_hMap, FILE_MAP_WRITE | FILE_MAP_READ | FILE_MAP_EXECUTE, NULL, NULL, NULL);
			}
		}

		Server::~Server() {
			if (m_pAddress) {
				UnmapViewOfFile(m_pAddress);
			}

			if (m_hMap && (m_hMap != INVALID_HANDLE_VALUE)) {
				CloseHandle(m_hMap);
			}
		}

		bool Server::GetSessionName(TCHAR szSessionName[64]) {
			if (!m_hMap || (m_hMap == INVALID_HANDLE_VALUE)) {
				return false;
			}

			memcpy(szSessionName, m_szSessionName, sizeof(m_szSessionName));

			return true;
		}

		void* Server::GetAddress() {
			return m_pAddress;
		}

		// ----------------------------------------------------------------
		// Client
		// ----------------------------------------------------------------

		Client::Client(TCHAR szSessionName[64], bool bIsGlobal) {
			m_hMap = nullptr;
			m_pAddress = nullptr;

			if (!szSessionName) {
				return;
			}

			TCHAR szMap[64];
			memset(szMap, 0, sizeof(szMap));
			if (bIsGlobal) {
				_stprintf_s(szMap, _T("Global\\%s"), szSessionName);
			} else {
				_stprintf_s(szMap, _T("Local\\%s"), szSessionName);
			}

			m_hMap = OpenFileMapping(FILE_MAP_WRITE | FILE_MAP_READ | FILE_MAP_EXECUTE, FALSE, szMap);
			if (m_hMap && (m_hMap != INVALID_HANDLE_VALUE)) {
				m_pAddress = MapViewOfFile(m_hMap, FILE_MAP_WRITE | FILE_MAP_READ | FILE_MAP_EXECUTE, NULL, NULL, NULL);
			}
		}

		Client::~Client() {
			if (m_pAddress) {
				UnmapViewOfFile(m_pAddress);
			}

			if (m_hMap && (m_hMap != INVALID_HANDLE_VALUE)) {
				CloseHandle(m_hMap);
			}
		}

		void* Client::GetAddress() {
			return m_pAddress;
		}

		// ----------------------------------------------------------------
		// Protection
		// ----------------------------------------------------------------

		Protection::Protection(const void* const pAddress, const size_t unSize) : m_pAddress(pAddress), m_unSize(unSize) {
			m_VirtualProtect = nullptr;
			m_unOriginalProtection = 0;

			if (!pAddress || !unSize) {
				return;
			}

			const HMODULE hKernelBase = GetModuleHandle(_T("KernelBase.dll"));
			if (!hKernelBase) {
				return;
			}

			m_VirtualProtect = reinterpret_cast<fnVirtualProtect>(GetProcAddress(hKernelBase, "VirtualProtect"));
			if (!m_VirtualProtect) {
				return;
			}

			MEMORY_BASIC_INFORMATION mbi;
			memset(&mbi, 0, sizeof(mbi));

			if (!VirtualQuery(m_pAddress, &mbi, sizeof(mbi))) {
				return;
			}

			m_unOriginalProtection = mbi.Protect;
		}

		Protection::~Protection() {
			if (!m_pAddress || !m_unSize || !m_VirtualProtect) {
				return;
			}

			DWORD unProtection = 0;
			m_VirtualProtect(const_cast<void*>(m_pAddress), m_unSize, m_unOriginalProtection, &unProtection);
		}

		bool Protection::GetProtection(const PDWORD pProtection) {
			if (!m_pAddress || !m_unSize || !m_VirtualProtect) {
				return false;
			}

			MEMORY_BASIC_INFORMATION mbi;
			memset(&mbi, 0, sizeof(mbi));

			if (!VirtualQuery(m_pAddress, &mbi, sizeof(mbi))) {
				return false;
			}

			if (pProtection) {
				*pProtection = mbi.Protect;
			}

			return true;
		}

		bool Protection::ChangeProtection(const DWORD unNewProtection) {
			if (!m_pAddress || !m_unSize || !m_VirtualProtect) {
				return false;
			}

			DWORD unProtection = 0;
			if (!m_VirtualProtect(const_cast<void*>(m_pAddress), m_unSize, unNewProtection, &unProtection)) {
				return false;
			}

			return true;
		}

		bool Protection::RestoreProtection() {
			if (!m_pAddress || !m_unSize || !m_VirtualProtect) {
				return false;
			}

			DWORD unProtection = 0;
			if (!m_VirtualProtect(const_cast<void*>(m_pAddress), m_unSize, m_unOriginalProtection, &unProtection)) {
				return false;
			}

			return true;
		}

		const void* const Protection::GetAddress() {
			return m_pAddress;
		}

		const size_t Protection::GetSize() {
			return m_unSize;
		}

		DWORD Protection::GetOriginalProtection() {
			return m_unOriginalProtection;
		}

		// ----------------------------------------------------------------
		// Simple Protection
		// ----------------------------------------------------------------

		bool ChangeProtection(const void* const pAddress, const size_t unSize, const DWORD unNewProtection) {
			if (!pAddress || !unSize) {
				return false;
			}

			const auto& it = g_Protections.find(const_cast<void*>(pAddress));
			if (it != g_Protections.end()) {
				return false;
			}

			auto pMemory = std::make_unique<Protection>(pAddress, unSize);
			if (!pMemory) {
				return false;
			}

			const void* const pMemoryAddress = pMemory->GetAddress();
			if (!pMemoryAddress) {
				return false;
			}

			if (!pMemory->ChangeProtection(unNewProtection)) {
				return false;
			}

			g_Protections.emplace_hint(it, const_cast<void*>(pAddress), std::move(pMemory));

			return true;
		}

		bool RestoreProtection(const void* const pAddress) {
			if (!pAddress) {
				return false;
			}

			const auto& it = g_Protections.find(const_cast<void*>(pAddress));
			if (it == g_Protections.end()) {
				return false;
			}

			g_Protections.erase(it);

			return true;
		}
	}

	// ----------------------------------------------------------------
	// Exception
	// ----------------------------------------------------------------

	namespace Exception {

		// ----------------------------------------------------------------
		// ExceptionHandler
		// ----------------------------------------------------------------

		static LONG NTAPI ExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) noexcept {
			if (!pExceptionInfo) {
				return EXCEPTION_ACCESS_VIOLATION;
			}

			const PEXCEPTION_RECORD pException = pExceptionInfo->ExceptionRecord;
			if (!pException) {
				return EXCEPTION_ACCESS_VIOLATION;
			}

			const PCONTEXT pCTX = pExceptionInfo->ContextRecord;
			if (!pCTX) {
				return EXCEPTION_ACCESS_VIOLATION;
			}

			const EXCEPTION_RECORD Exception = *pException;
			auto& vecCallBacks = g_ExceptionListener.GetCallBacks();
			for (auto it = vecCallBacks.begin(); it != vecCallBacks.end(); ++it) {
				const auto& CallBack = (*it);
				if (!CallBack) {
					vecCallBacks.erase(it);
					continue;
				}

				if (CallBack(Exception, pCTX)) {
					return EXCEPTION_CONTINUE_EXECUTION;
				}
			}

			return EXCEPTION_CONTINUE_SEARCH;
		}

		// ----------------------------------------------------------------
		// MemoryHookCallBack
		// ----------------------------------------------------------------

		static bool __fastcall MemoryHookCallBack(const EXCEPTION_RECORD& Exception, const PCONTEXT pCTX) {
			for (auto it = g_MemoryHooks.begin(); it != g_MemoryHooks.end(); ++it) {
				const auto& pHook = it->second;
				if (!pHook) {
					continue;
				}

				if (pHook->GetAddress() != Exception.ExceptionAddress) {
					continue;
				}

				if (pHook->IsAutoDisable()) {
					pHook->Disable();
				}

				const auto& pCallBack = pHook->GetCallBack();
				if (!pCallBack) {
					return false;
				}

				return pCallBack(pHook, pCTX);
			}

			return false;
		}

		// ----------------------------------------------------------------
		// ExceptionListener
		// ----------------------------------------------------------------

		ExceptionListener::ExceptionListener() {
			m_pVEH = AddVectoredExceptionHandler(TRUE, ExceptionHandler);
			if (m_pVEH) {

				// CallBacks
				AddCallBack(MemoryHookCallBack);
			}
		}

		ExceptionListener::~ExceptionListener() {
			if (m_pVEH) {
				RemoveVectoredExceptionHandler(m_pVEH);

				// CallBacks
				RemoveCallBack(MemoryHookCallBack);
			}
		}

		bool ExceptionListener::EnableHandler() {
			if (m_pVEH) {
				return false;
			}

			m_pVEH = AddVectoredExceptionHandler(TRUE, ExceptionHandler);
			return true;
		}

		bool ExceptionListener::DisableHandler() {
			if (!m_pVEH) {
				return false;
			}

			RemoveVectoredExceptionHandler(m_pVEH);
			m_pVEH = nullptr;
			return true;
		}

		bool ExceptionListener::RefreshHandler() {
			if (!DisableHandler()) {
				return false;
			}

			if (!EnableHandler()) {
				return false;
			}

			return true;
		}

		bool ExceptionListener::AddCallBack(const fnExceptionCallBack pCallBack) {
			if (!pCallBack) {
				return false;
			}

			for (auto it = m_vecCallBacks.begin(); it != m_vecCallBacks.end(); ++it) {
				if (pCallBack == *it) {
					return false;
				}
			}

			m_vecCallBacks.push_back(pCallBack);
			return true;
		}

		bool ExceptionListener::RemoveCallBack(const fnExceptionCallBack pCallBack) {
			if (!pCallBack) {
				return false;
			}

			for (auto it = m_vecCallBacks.begin(); it != m_vecCallBacks.end(); ++it) {
				if (pCallBack == *it) {
					m_vecCallBacks.erase(it);
					return true;
				}
			}

			return false;
		}

		std::deque<fnExceptionCallBack>& ExceptionListener::GetCallBacks() {
			return m_vecCallBacks;
		}

		ExceptionListener g_ExceptionListener;
	}

	// ----------------------------------------------------------------
	// Hook
	// ----------------------------------------------------------------

	namespace Hook {

		// ----------------------------------------------------------------
		// Memory Hook
		// ----------------------------------------------------------------

		MemoryHook::MemoryHook(const void* const pAddress, const size_t unSize, bool bAutoDisable) : m_pAddress(pAddress), m_unSize(unSize) {
			m_bAutoDisable = bAutoDisable;
			m_pCallBack = nullptr;
		}

		MemoryHook::~MemoryHook() {
			UnHook();
		}

		bool MemoryHook::Hook(const fnMemoryHookCallBack pCallBack) {
			if (!m_pAddress || !m_unSize || m_pCallBack) {
				return false;
			}

			m_pCallBack = pCallBack;

			const auto& it = g_Protections.find(const_cast<void*>(m_pAddress));
			if (it != g_Protections.end()) {
				return false;
			}

			auto pProtection = std::make_unique<Protection>(m_pAddress, m_unSize);
			if (!pProtection) {
				return false;
			}

			DWORD unProtection = 0;
			if (!pProtection->GetProtection(&unProtection)) {
				return false;
			}

			if (unProtection & PAGE_EXECUTE) {
				unProtection &= ~PAGE_EXECUTE;
			}

			if (unProtection & PAGE_EXECUTE_READ) {
				unProtection &= ~PAGE_EXECUTE_READ;
			}

			if (unProtection & PAGE_EXECUTE_READWRITE) {
				unProtection &= ~PAGE_EXECUTE_READWRITE;
			}

			if (!unProtection) {
				unProtection |= PAGE_READONLY;
			}

			if (!pProtection->ChangeProtection(unProtection)) {
				return false;
			}

			g_Protections.emplace_hint(it, const_cast<void*>(m_pAddress), std::move(pProtection));

			return true;
		}

		bool MemoryHook::UnHook() {
			if (!m_pAddress || !m_unSize || !m_pCallBack) {
				return false;
			}

			const auto& it = g_Protections.find(const_cast<void*>(m_pAddress));
			if (it == g_Protections.end()) {
				return false;
			}

			const auto& pProtection = it->second;
			if (!pProtection) {
				return false;
			}

			if (!pProtection->RestoreProtection()) {
				return false;
			}

			g_Protections.erase(it);

			return true;
		}

		bool MemoryHook::Enable() {
			if (!m_pAddress || !m_unSize || !m_pCallBack) {
				return false;
			}

			const auto& it = g_Protections.find(const_cast<void*>(m_pAddress));
			if (it == g_Protections.end()) {
				return false;
			}

			const auto& pProtection = it->second;
			if (!pProtection) {
				return false;
			}

			DWORD unProtection = 0;
			if (!pProtection->GetProtection(&unProtection)) {
				return false;
			}

			if (unProtection & PAGE_EXECUTE) {
				unProtection &= ~PAGE_EXECUTE;
			}

			if (unProtection & PAGE_EXECUTE_READ) {
				unProtection &= ~PAGE_EXECUTE_READ;
			}

			if (unProtection & PAGE_EXECUTE_READWRITE) {
				unProtection &= ~PAGE_EXECUTE_READWRITE;
			}

			if (!unProtection) {
				unProtection |= PAGE_READONLY;
			}

			return pProtection->ChangeProtection(unProtection);
		}

		bool MemoryHook::Disable() {
			if (!m_pAddress || !m_unSize || !m_pCallBack) {
				return false;
			}

			const auto& it = g_Protections.find(const_cast<void*>(m_pAddress));
			if (it == g_Protections.end()) {
				return false;
			}

			const auto& pProtection = it->second;
			if (!pProtection) {
				return false;
			}

			return pProtection->RestoreProtection();
		}

		const void* const MemoryHook::GetAddress() {
			return m_pAddress;
		}

		const size_t MemoryHook::GetSize() {
			return m_unSize;
		}

		bool MemoryHook::IsAutoDisable() {
			return m_bAutoDisable;
		}

		fnMemoryHookCallBack MemoryHook::GetCallBack() {
			return m_pCallBack;
		}

		// ----------------------------------------------------------------
		// Simple Memory Hook
		// ----------------------------------------------------------------

		bool HookMemory(const void* const pAddress, const fnMemoryHookCallBack pCallBack, bool bAutoDisable) {
			if (!pAddress || !pCallBack) {
				return false;
			}

			const auto& it = g_MemoryHooks.find(pCallBack);
			if (it != g_MemoryHooks.end()) {
				return false;
			}

			auto pHook = std::make_unique<MemoryHook>(pAddress, 1, bAutoDisable);
			if (!pHook) {
				return false;
			}

			if (!pHook->Hook(pCallBack)) {
				return false;
			}

			g_MemoryHooks.emplace_hint(it, pCallBack, std::move(pHook));

			return true;
		}

		bool UnHookMemory(const fnMemoryHookCallBack pCallBack) {
			if (!pCallBack) {
				return false;
			}

			const auto& it = g_MemoryHooks.find(pCallBack);
			if (it == g_MemoryHooks.end()) {
				return false;
			}

			const auto& pHook = it->second;
			if (!pHook) {
				return false;
			}

			return pHook->UnHook();
		}

		bool EnableHookMemory(const fnMemoryHookCallBack pCallBack) {
			if (!pCallBack) {
				return false;
			}

			const auto& it = g_MemoryHooks.find(pCallBack);
			if (it == g_MemoryHooks.end()) {
				return false;
			}

			const auto& pHook = it->second;
			if (!pHook) {
				return false;
			}

			return pHook->Enable();
		}

		bool DisableHookMemory(const fnMemoryHookCallBack pCallBack) {
			if (!pCallBack) {
				return false;
			}

			const auto& it = g_MemoryHooks.find(pCallBack);
			if (it == g_MemoryHooks.end()) {
				return false;
			}

			const auto& pHook = it->second;
			if (!pHook) {
				return false;
			}

			return pHook->Disable();
		}
	}
}
