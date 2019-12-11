/*
 * Tiny PNG Output (C++)
 *
 * Copyright (c) 2018 Project Nayuki
 * https://www.nayuki.io/page/tiny-png-output
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program (see COPYING.txt and COPYING.LESSER.txt).
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>
#include "cm_ctors.h"

/*
 * Takes image pixel data in raw RGB8.8.8 format and writes a PNG file to a byte output stream.
 * Writer must be callable which take pointer to data and size to write.
 */
template <class Writer>
class TinyPngOut final
{
    /*---- Fields ----*/

    // Immutable configuration
private:
    std::uint32_t width;   // Measured in pixels
    std::uint32_t height;  // Measured in pixels
    std::uint32_t lineSize;  // Measured in bytes, equal to (width * 3 + 1)

    // Running state
    const Writer &output;
    std::uint32_t positionX{0};      // Next byte index in current line
    std::uint32_t positionY{0};      // Line index of next byte
    std::uint32_t uncompRemain{0};   // Number of uncompressed bytes remaining

    std::uint16_t deflateFilled{0};  // Bytes filled in the current block (0 <= n < DEFLATE_MAX_BLOCK_SIZE)
    std::uint32_t crc{0};    // Primarily for IDAT chunk
    std::uint32_t adler{1};  // For DEFLATE data within IDAT



    /*---- Public constructor and method ----*/

    /*
     * Creates a PNG writer with the given width and height (both non-zero) and byte output stream.
     * TinyPngOut will leave the output stream still open once it finishes writing the PNG file data.
     * Throws an exception if the dimensions exceed certain limits (e.g. w * h > 700 million).
     */
public:
    TinyPngOut() = delete;
    NO_COPYMOVE(TinyPngOut);
    NO_NEW;

    explicit TinyPngOut(std::uint32_t w, std::uint32_t h, const Writer &out) :
        // Set most of the fields
        width(w),
        height(h),
        output(out)
    {

        // Check arguments
        if (width == 0 || height == 0)
            throw std::domain_error("Zero width or height");

        // Compute and check data siezs
        auto lineSz = static_cast<std::uint64_t>(width) * 3 + 1;
        if (lineSz > std::numeric_limits<uint32_t>::max())
            throw std::length_error("Image too large");
        lineSize = static_cast<uint32_t>(lineSz);

        uint64_t uncompRm = lineSize * height;
        if (uncompRm > UINT32_MAX)
            throw std::length_error("Image too large");
        uncompRemain = static_cast<uint32_t>(uncompRm);

        uint32_t numBlocks = uncompRemain / DEFLATE_MAX_BLOCK_SIZE;
        if (uncompRemain % DEFLATE_MAX_BLOCK_SIZE != 0)
            numBlocks++;  // Round up
        // 5 bytes per DEFLATE uncompressed block header, 2 bytes for zlib header, 4 bytes for zlib Adler-32 footer
        uint64_t idatSize = static_cast<uint64_t>(numBlocks) * 5 + 6;
        idatSize += uncompRemain;
        if (idatSize > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
            throw std::length_error("Image too large");

        // Write header (not a pure header, but a couple of things concatenated together)
        uint8_t header[] =    // 43 bytes long
        {
            // PNG header
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
            // IHDR chunk
            0x00, 0x00, 0x00, 0x0D,
            0x49, 0x48, 0x44, 0x52,
            0, 0, 0, 0,  // 'width' placeholder
            0, 0, 0, 0,  // 'height' placeholder
            0x08, 0x02, 0x00, 0x00, 0x00,
            0, 0, 0, 0,  // IHDR CRC-32 placeholder
            // IDAT chunk
            0, 0, 0, 0,  // 'idatSize' placeholder
            0x49, 0x44, 0x41, 0x54,
            // DEFLATE data
            0x08, 0x1D,
        };
        putBigUint32(width, &header[16]);
        putBigUint32(height, &header[20]);
        putBigUint32(idatSize, &header[33]);
        crc = 0;
        crc32(&header[12], 17);
        putBigUint32(crc, &header[29]);
        write(header);

        crc = 0;
        crc32(&header[37], 6);  // 0xD7245B6B
    }


    /*
     * Writes 'count' pixels from the given array to the output stream. This reads count*3
     * bytes from the array. Pixels are presented from top to bottom, left to right, and with
     * subpixels in RGB order. This object keeps track of how many pixels were written and
     * various position variables. It is an error to write more pixels in total than width*height.
     * Once exactly width*height pixels have been written with this TinyPngOut object,
     * there are no more valid operations on the object and it should be discarded.
     */
public:

    template <class Alloc>
    void write(const std::vector<uint8_t, Alloc>& src)
    {
        const size_t pixels_count = width * height;
        if (pixels_count * 3 > src.size())
            throw std::length_error("Amount of expected pixels bigger then buffer.");
        write(src.data(), pixels_count);
    }

    void write(const uint8_t* pixels, size_t count)
    {
        if (count > SIZE_MAX / 3)
            throw std::length_error("Invalid argument");
        count *= 3;  // Convert pixel count to byte count
        while (count > 0)
        {
            if (pixels == nullptr)
                throw std::invalid_argument("Null pointer");
            if (positionY >= height)
                throw std::logic_error("All image pixels already written");

            if (deflateFilled == 0)    // Start DEFLATE block
            {
                uint16_t size = DEFLATE_MAX_BLOCK_SIZE;
                if (uncompRemain < size)
                    size = static_cast<uint16_t>(uncompRemain);
                const uint8_t header[] =    // 5 bytes long
                {
                    static_cast<uint8_t>(uncompRemain <= DEFLATE_MAX_BLOCK_SIZE ? 1 : 0),
                    static_cast<uint8_t>(size >> 0),
                    static_cast<uint8_t>(size >> 8),
                    static_cast<uint8_t>((size >> 0) ^ 0xFF),
                    static_cast<uint8_t>((size >> 8) ^ 0xFF),
                };
                write(header);
                crc32(header);
            }
            assert(positionX < lineSize && deflateFilled < DEFLATE_MAX_BLOCK_SIZE);

            if (positionX == 0)    // Beginning of line - write filter method byte
            {
                const static uint8_t b[1] = {0};
                write(b);
                crc32(b);
                adler32(b);
                positionX++;
                uncompRemain--;
                deflateFilled++;

            }
            else      // Write some pixel bytes for current line
            {
                uint16_t n = DEFLATE_MAX_BLOCK_SIZE - deflateFilled;
                if (lineSize - positionX < n)
                    n = static_cast<uint16_t>(lineSize - positionX);
                if (count < n)
                    n = static_cast<uint16_t>(count);
                if (static_cast<std::make_unsigned<std::streamsize>::type>(std::numeric_limits<std::streamsize>::max()) < std::numeric_limits<decltype(n)>::max())
                    n = std::min(n, static_cast<decltype(n)>(std::numeric_limits<std::streamsize>::max()));
                assert(n > 0);
                output(pixels, n);

                // Update checksums
                crc32(pixels, n);
                adler32(pixels, n);

                // Increment positions
                count -= n;
                pixels += n;
                positionX += n;
                uncompRemain -= n;
                deflateFilled += n;
            }

            if (deflateFilled >= DEFLATE_MAX_BLOCK_SIZE)
                deflateFilled = 0;  // End current block

            if (positionX == lineSize)    // Increment line
            {
                positionX = 0;
                positionY++;
                if (positionY == height)    // Reached end of pixels
                {
                    uint8_t footer[] =    // 20 bytes long
                    {
                        0, 0, 0, 0,  // DEFLATE Adler-32 placeholder
                        0, 0, 0, 0,  // IDAT CRC-32 placeholder
                        // IEND chunk
                        0x00, 0x00, 0x00, 0x00,
                        0x49, 0x45, 0x4E, 0x44,
                        0xAE, 0x42, 0x60, 0x82,
                    };
                    putBigUint32(adler, footer + 0);
                    crc32(footer, 4);
                    putBigUint32(crc, footer + 4);
                    write(footer);
                }
            }
        }
    }



    /*---- Private checksum methods ----*/

    // Reads the 'crc' field and updates its value based on the given array of new data.
private:
    void crc32(const uint8_t* data, size_t len)
    {
        constexpr uint32_t magic1 = 0xEDB88320;

        crc = ~crc;
        for (size_t i = 0; i < len; ++i)
        {
            for (int j = 0; j < 8; ++j)    // Inefficient bitwise implementation, instead of table-based
            {
                uint32_t bit = (crc ^ (data[i] >> j)) & 1;
                crc = (crc >> 1) ^ ((-bit) & magic1);
            }
        }
        crc = ~crc;
    }
    template <size_t N>
    void crc32(const uint8_t (&data)[N])
    {
        crc32(data, N);
    }

    // Reads the 'adler' field and updates its value based on the given array of new data.
    void adler32(const uint8_t* data, size_t len)
    {
        constexpr uint32_t mask = 0xFFFF;
        constexpr uint32_t div  = 65521u;

        uint32_t s1 = adler & mask;
        uint32_t s2 = adler >> 16;
        for (size_t i = 0; i < len; ++i)
        {
            s1 = (s1 + data[i]) % div;
            s2 = (s2 + s1) % div;
        }
        adler = (s2 << 16) | s1;
    }

    template <size_t N>
    void adler32(const uint8_t (&data)[N])
    {
        adler32(data, N);
    }

    /*---- Private utility members ----*/


    template <size_t N>
    void write(const uint8_t (&data)[N])
    {
        output(data, N);
    }

    static void putBigUint32(std::uint32_t val, std::uint8_t array[4])
    {
        for (int i = 0; i < 4; ++i)
            array[i] = static_cast<uint8_t>(val >> ((3 - i) * 8));
    }

    static constexpr std::uint16_t DEFLATE_MAX_BLOCK_SIZE = 65535;

};
