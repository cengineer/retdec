/*
* ImageLoader.h - Interface to the PE imaage loader class
*
* Copyright (c) 2020 Ladislav Zezula
* All rights reserved.
*
* This software is licensed under the zlib/libpng License.
* For more details see http://www.opensource.org/licenses/zlib-license.php
* or the license information file (license.htm) in the root directory
* of PeLib.
*/

#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <string>
#include <vector>

#include "PeLibAux.h"

namespace PeLib {

//-----------------------------------------------------------------------------
// Enum for Windows loader emulation

enum : std::uint32_t
{
	LoaderModeWindowsXP = 0x51,          // Behavior equal to Windows XP
	LoaderModeWindows7  = 0x61,          // Behavior equal to Windows 7
	LoaderModeWindows10 = 0xA0,          // Behavior equal to Windows 10
	WindowsVerMask = 0x0FFF,             // Mask for extracting the operating system
	LoaderMode64BitWindows = 0x1000,     // Emulate 64-bit system
};

//-----------------------------------------------------------------------------
// Support structure for one PE image compare result

enum PELIB_COMPARE_RESULT : std::uint32_t
{
	ImagesEqual,                                // The images are equal
	ImagesWindowsLoadedWeDidnt,                 // 
	ImagesWindowsDidntLoadWeDid,                // 
	ImagesDifferentSize,                        // The images have different size
	ImagesDifferentPageAccess,                  // An image page is different (accessible vs non-accessible)
	ImagesDifferentPageValue,                   // There is a different value at a certain offset
};

typedef bool (*PFN_VERIFY_ADDRESS)(void * ptr, size_t length);
typedef bool (*PFN_COMPARE_CALLBACK)(size_t BytesCompared, size_t BytesTotal);

struct PELIB_IMAGE_COMPARE
{
	PFN_VERIFY_ADDRESS PfnVerifyAddress;       // Custom function for verifying memory address
	PFN_COMPARE_CALLBACK PfnCompareCallback;   // Custom function for calling compare callback
	PELIB_COMPARE_RESULT compareResult;
	const char * dumpIfNotEqual;               // If non-NULL, the image will be dumped into that file
	std::uint32_t differenceOffset;
};

//-----------------------------------------------------------------------------
// Support structure for one PE file page

struct PELIB_FILE_PAGE
{
	PELIB_FILE_PAGE()
	{
		isInvalidPage = true;
		isZeroPage = false;
	}

	~PELIB_FILE_PAGE()
	{}

	// Initializes the page with a valid data
	bool setValidPage(const void * data, size_t length)
	{
		// Write the valid data to the page
		writeToPage(data, 0, length);

		// Write zero data to the end of the page
		memset(buffer.data() + length, 0, PELIB_PAGE_SIZE - length);

		isInvalidPage = false;
		isZeroPage = false;
		return true;
	}

	// Initializes the page as zero page. To save memory, we won't initialize buffer
	void setZeroPage()
	{
		buffer.clear();
		isInvalidPage = false;
		isZeroPage = true;
	}

	void writeToPage(const void * data, size_t offset, size_t length)
	{
		if(offset < PELIB_PAGE_SIZE)
		{
			// Make sure that there is buffer allocated
			if(buffer.size() != PELIB_PAGE_SIZE)
				buffer.resize(PELIB_PAGE_SIZE);

			// Copy the data, up to page size
			if((offset + length) > PELIB_PAGE_SIZE)
				length = PELIB_PAGE_SIZE - offset;
			memcpy(buffer.data() + offset, data, length);
		}
	}

	std::vector<std::uint8_t> buffer;     // A page-sized buffer, holding one image page. Empty if isInvalidPage
	bool isInvalidPage;                   // For invalid pages within image (SectionAlignment > 0x1000)
	bool isZeroPage;                      // For sections with VirtualSize != 0, RawSize = 0
};

//-----------------------------------------------------------------------------
// Image loader class interface

class ImageLoader
{
	public:

	ImageLoader(std::uint32_t loaderFlags = 0);
	~ImageLoader();

	int Load(std::vector<std::uint8_t> & fileData, bool loadHeadersOnly = false);
	int Load(std::istream & fs, std::streamoff fileOffset = 0, bool loadHeadersOnly = false);
	int Load(const char * fileName, bool loadHeadersOnly = false);

	bool relocateImage(std::uint64_t newImageBase);

	std::uint32_t readImage(void * buffer, std::uint32_t rva, std::uint32_t bytesToRead);
	std::uint32_t writeImage(void * buffer, std::uint32_t rva, std::uint32_t bytesToRead);

	std::uint32_t readString(std::string & str, std::uint32_t rva, std::uint32_t maxLength = 65535);
	std::uint32_t readStringRc(std::string & str, std::uint32_t rva);
	std::uint32_t readStringRaw(std::vector<std::uint8_t> & fileData, std::string & str, std::size_t offset, std::size_t maxLength = 65535, bool mustBePrintable = false, bool mustNotBeTooLong = false);
	std::uint32_t stringLength(std::uint32_t rva, std::uint32_t maxLength = 65535) const;

	std::uint32_t readPointer(std::uint32_t rva, std::uint64_t & pointerValue);
	std::uint32_t getPointerSize() const;

	std::uint32_t dumpImage(const char * fileName);

	std::uint32_t getImageBitability() const;

	std::uint32_t getFileOffsetFromRva(std::uint32_t rva) const;
	std::uint32_t getRealPointerToRawData(std::size_t sectionIndex) const;
	std::uint32_t getImageProtection(std::uint32_t characteristics) const;

	bool setDataDirectory(std::uint32_t index, std::uint32_t rva, std::uint32_t size);

	const PELIB_IMAGE_DOS_HEADER & getDosHeader() const
	{
		return dosHeader;
	}

	const PELIB_IMAGE_FILE_HEADER & getFileHeader() const
	{
		return fileHeader;
	}

	const PELIB_IMAGE_OPTIONAL_HEADER & getOptionalHeader() const
	{
		return optionalHeader;
	}

	const PELIB_SECTION_HEADER * getSectionHeader(std::size_t sectionIndex) const
	{
		return (sectionIndex < sections.size()) ? &sections[sectionIndex] : nullptr;
	}

	std::uint64_t getOrdinalMask() const
	{
		return (uint64_t)1 << (getImageBitability() - 1);
	}

	std::uint32_t getPeHeaderOffset() const
	{
		return dosHeader.e_lfanew;
	}

	std::uint32_t getNtSignature() const
	{
		return ntSignature;
	}

	std::uint32_t getMachine() const
	{
		return fileHeader.Machine;
	}

	std::uint32_t getPointerToSymbolTable() const
	{
		return fileHeader.PointerToSymbolTable;
	}

	std::uint32_t getNumberOfSymbols() const
	{
		return fileHeader.NumberOfSymbols;
	}

	std::uint32_t getLoadedNumberOfSections() const
	{
		return fileHeader.NumberOfSections;
	}

	std::uint32_t getCharacteristics() const
	{
		return fileHeader.Characteristics;
	}

	std::uint32_t getNumberOfSections() const
	{
		return sections.size();
	}

	std::uint32_t getMagic() const
	{
		return optionalHeader.Magic;
	}

	std::uint64_t getImageBase() const
	{
		return optionalHeader.ImageBase;
	}

	std::uint32_t getAddressOfEntryPoint() const
	{
		return optionalHeader.AddressOfEntryPoint;
	}

	std::uint32_t getSizeOfHeaders() const
	{
		return optionalHeader.SizeOfHeaders;
	}

	std::uint32_t getSizeOfImage() const
	{
		return optionalHeader.SizeOfImage;
	}

	std::uint32_t getSizeOfImageAligned() const
	{
		return AlignToSize(optionalHeader.SizeOfImage, PELIB_PAGE_SIZE);
	}

	std::uint32_t getSectionAlignment() const
	{
		return optionalHeader.SectionAlignment;
	}

	std::uint32_t getFileAlignment() const
	{
		return optionalHeader.FileAlignment;
	}

	std::uint32_t getChecksumFileOffset() const
	{
		return checkSumFileOffset;
	}

	std::uint32_t getRealNumberOfDataDirectories() const
	{
		return realNumberOfRvaAndSizes;
	}

	std::uint32_t getSecurityDirFileOffset() const
	{
		return securityDirFileOffset;
	}

	std::uint32_t getDataDirRva(std::size_t dataDirIndex) const
	{
		// The data directory must be present there
		return (optionalHeader.NumberOfRvaAndSizes > dataDirIndex) ? optionalHeader.DataDirectory[dataDirIndex].VirtualAddress : 0;
	}

	std::uint32_t getDataDirSize(std::size_t dataDirIndex) const
	{
		// The data directory must be present there
		return (optionalHeader.NumberOfRvaAndSizes > dataDirIndex) ? optionalHeader.DataDirectory[dataDirIndex].Size : 0;
	}

	std::uint64_t getVirtualAddressMasked(std::uint32_t rva)
	{
		std::uint64_t virtualAddress = getImageBase() + rva;

		if(getImageBitability() == 32)
			virtualAddress = virtualAddress & 0xFFFFFFFF;
		return virtualAddress;
	}

	int setLoaderError(LoaderError ldrErr);
	LoaderError loaderError() const;

	// Testing functions
	std::size_t getMismatchOffset(void * buffer1, void * buffer2, std::uint32_t rva, std::size_t length);
	void compareWithWindowsMappedImage(PELIB_IMAGE_COMPARE & ImageCompare, void * imageData, std::uint32_t imageSize);

	protected:

	typedef void (*READWRITE)(PeLib::PELIB_FILE_PAGE & page, void * buffer, std::size_t offsetInPage, std::size_t bytesInPage);

	static void readFromPage(PELIB_FILE_PAGE & page, void * buffer, size_t offsetInPage, size_t bytesInPage);
	static void writeToPage(PELIB_FILE_PAGE & page, void * buffer, size_t offsetInPage, size_t bytesInPage);
	std::uint32_t readWriteImage(void * buffer, std::uint32_t rva, std::uint32_t bytesToRead, READWRITE ReadWrite);
	std::uint32_t readWriteImageFile(void * buffer, std::uint32_t rva, std::uint32_t bytesToRead, bool bReadOperation);

	bool processImageRelocation_IA64_IMM64(std::uint32_t fixupAddress, std::uint64_t difference);
	bool processImageRelocations(std::uint64_t oldImageBase, std::uint64_t getImageBase, std::uint32_t VirtualAddress, std::uint32_t Size);
	void writeNewImageBase(std::uint64_t newImageBase);

	int captureDosHeader(std::vector<std::uint8_t> & fileData);
	int captureNtHeaders(std::vector<std::uint8_t> & fileData);
	int captureSectionName(std::vector<std::uint8_t> & fileData, std::string & sectionName, const std::uint8_t * name);
	int captureSectionHeaders(std::vector<std::uint8_t> & fileData);
	int captureImageSections(std::vector<std::uint8_t> & fileData);
	int captureOptionalHeader32(std::uint8_t * fileData, std::uint8_t * filePtr, std::uint8_t * fileEnd);
	int captureOptionalHeader64(std::uint8_t * fileData, std::uint8_t * filePtr, std::uint8_t * fileEnd);

	int verifyDosHeader(PELIB_IMAGE_DOS_HEADER & hdr, std::size_t fileSize);
	int verifyDosHeader(std::istream & fs, std::streamoff fileOffset, std::size_t fileSize);

	int loadImageAsIs(std::vector<std::uint8_t> & fileData);

	std::uint32_t captureImageSection(std::vector<std::uint8_t> & fileData,
									  std::uint32_t virtualAddress,
									  std::uint32_t virtualSize,
									  std::uint32_t pointerToRawData,
									  std::uint32_t sizeOfRawData,
									  std::uint32_t characteristics,
									  bool isImageHeader = false);

	bool isGoodPagePointer(PFN_VERIFY_ADDRESS PfnVerifyAddress, void * pagePtr);
	bool isGoodMappedPage(std::uint32_t rva);
	bool isZeroPage(std::uint32_t rva);

	bool isRvaOfSectionHeaderPointerToRawData(uint32_t rva);
	bool isLegacyImageArchitecture(std::uint16_t Machine);
	bool checkForValid64BitMachine();
	bool checkForValid32BitMachine();
	bool checkForBadAppContainer();
	
	// isImageLoadable returns true if the image is OK and can be mapped by NtCreateSection(SEC_IMAGE).
	// This does NOT mean that the image is executable by CreateProcess - more checks are done,
	// like resource integrity or relocation table correctness.
	bool isImageLoadable() const;
	bool isImageMappedOk() const;
	bool isValidImageBlock(std::uint32_t Rva, std::uint32_t Size)  const;

	static std::uint32_t AlignToSize(std::uint32_t ByteSize, std::uint32_t AlignSize)
	{
		return ((ByteSize + (AlignSize - 1)) & ~(AlignSize - 1));
	}

	static std::uint32_t BytesToPages(std::uint32_t ByteSize)
	{
		return (ByteSize >> PELIB_PAGE_SIZE_SHIFT) + ((ByteSize & (PELIB_PAGE_SIZE - 1)) != 0);
	}

	static std::uint64_t signExtend32To64(std::uint32_t value32)
	{
		return (std::uint64_t)(std::int64_t)(std::int32_t)value32;
	}

	// Anti-assert feature. Debug version of isprint in MS Visual C++ asserts
	// when the character is not EOF or is >= 255
	bool isPrintableChar(int ch)
	{
		return ((EOF <= ch) && (ch <= 255)) ? isprint(ch) : false;
	}

	static uint8_t ImageProtectionArray[16];

	std::vector<PELIB_SECTION_HEADER> sections;         // Vector of section headers
	std::vector<PELIB_FILE_PAGE> pages;                 // PE file pages as if mapped
	std::vector<std::uint8_t> rawFileData;              // Loaded content of the image in case it couldn't have been mapped
	PELIB_IMAGE_DOS_HEADER  dosHeader;                  // Loaded DOS header
	PELIB_IMAGE_FILE_HEADER fileHeader;                 // Loaded NT file header
	PELIB_IMAGE_OPTIONAL_HEADER optionalHeader;         // 32/64-bit optional header
	LoaderError ldrError;
	std::uint32_t ntSignature;
	std::uint32_t loaderMode;
	std::uint32_t maxSectionCount;
	std::uint32_t realNumberOfRvaAndSizes;              // Real present number of RVA and sizes
	std::uint32_t checkSumFileOffset;                   // File offset of the image checksum
	std::uint32_t securityDirFileOffset;                // File offset of security directory
	bool ntHeadersSizeCheck;                            // If true, the loader requires minimum size of NT headers
	bool sizeofImageMustMatch;                          // If true, the SizeOfImage must match virtual end of the last section
	bool appContainerCheck;                             // If true, app container flag is tested in the optional header
	bool is64BitWindows;                                // If true, we simulate 64-bit Windows
	bool loadArmImages;                                 // If true, image loader will load ARM binaries
};

}	// namespace PeLib

#endif	// IMAGE_LOADER_H