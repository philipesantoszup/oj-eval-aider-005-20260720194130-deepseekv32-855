#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0; 
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {

    // qoi-header part

    // write magic bytes "qoif"
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    // write image width
    QoiWriteU32(width);
    // write image height
    QoiWriteU32(height);
    // write channel number
    QoiWriteU8(channels);
    // write color space specifier
    QoiWriteU8(colorspace);

    /* qoi-data part */
    int run = 0;
    int px_num = width * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r, g, b, a;
    uint8_t pre_r = 0u;
    uint8_t pre_g = 0u;
    uint8_t pre_b = 0u;
    uint8_t pre_a = 255u;

    for (int i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        if (std::cin.eof()) return false;
        g = QoiReadU8();
        if (std::cin.eof()) return false;
        b = QoiReadU8();
        if (std::cin.eof()) return false;
        if (channels == 4) {
            a = QoiReadU8();
            if (std::cin.eof()) return false;
        } else {
            a = 255u;
        }

        // Check for RUN
        if (r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            run++;
            if (run == 62 || i == px_num - 1) {
                QoiWriteU8(QOI_OP_RUN_TAG | (run - 1));
                run = 0;
            }
        } else {
            // Flush any pending run
            if (run > 0) {
                QoiWriteU8(QOI_OP_RUN_TAG | (run - 1));
                run = 0;
            }

            // Compute hash
            int index = QoiColorHash(r, g, b, a);
            
            // Check for INDEX
            if (history[index][0] == r && history[index][1] == g && 
                history[index][2] == b && history[index][3] == a) {
                QoiWriteU8(QOI_OP_INDEX_TAG | index);
            } else {
                // Update history
                history[index][0] = r;
                history[index][1] = g;
                history[index][2] = b;
                history[index][3] = a;

                // Compute differences
                int dr = (int)r - (int)pre_r;
                int dg = (int)g - (int)pre_g;
                int db = (int)b - (int)pre_b;
                int da = (int)a - (int)pre_a;

                // Check for DIFF
                if (dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1 && da == 0) {
                    uint8_t op = QOI_OP_DIFF_TAG | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2);
                    QoiWriteU8(op);
                } else {
                    // Check for LUMA
                    int dr_dg = dr - dg;
                    int db_dg = db - dg;
                    if (dg >= -32 && dg <= 31 && dr_dg >= -8 && dr_dg <= 7 && db_dg >= -8 && db_dg <= 7 && da == 0) {
                        QoiWriteU8(QOI_OP_LUMA_TAG | (dg + 32));
                        QoiWriteU8(((dr_dg + 8) << 4) | (db_dg + 8));
                    } else {
                        // Use RGB or RGBA
                        if (da == 0) {
                            QoiWriteU8(QOI_OP_RGB_TAG);
                            QoiWriteU8(r);
                            QoiWriteU8(g);
                            QoiWriteU8(b);
                        } else {
                            QoiWriteU8(QOI_OP_RGBA_TAG);
                            QoiWriteU8(r);
                            QoiWriteU8(g);
                            QoiWriteU8(b);
                            QoiWriteU8(a);
                        }
                    }
                }
            }
        }

        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }
    // Flush any remaining run
    if (run > 0) {
        QoiWriteU8(QOI_OP_RUN_TAG | (run - 1));
    }

    // qoi-padding part
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {

    uint8_t c1 = QoiReadU8();
    uint8_t c2 = QoiReadU8();
    uint8_t c3 = QoiReadU8();
    uint8_t c4 = QoiReadU8();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    // read image width
    width = QoiReadU32();
    // read image height
    height = QoiReadU32();
    // read channel number
    channels = QoiReadU8();
    // read color space specifier
    colorspace = QoiReadU8();

    int px_num = width * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r = 0, g = 0, b = 0, a = 255;
    uint8_t pre_r = 0, pre_g = 0, pre_b = 0, pre_a = 255;

    for (int i = 0; i < px_num; ++i) {
        uint8_t op = QoiReadU8();
        if (std::cin.eof()) return false;
        
        if (op == QOI_OP_RGB_TAG) {
            r = QoiReadU8();
            if (std::cin.eof()) return false;
            g = QoiReadU8();
            if (std::cin.eof()) return false;
            b = QoiReadU8();
            if (std::cin.eof()) return false;
            // For RGB images, alpha is always 255
            if (channels == 3) {
                a = 255;
            } else {
                // For RGBA images, alpha remains unchanged from previous pixel
                a = pre_a;
            }
        } else if (op == QOI_OP_RGBA_TAG) {
            r = QoiReadU8();
            if (std::cin.eof()) return false;
            g = QoiReadU8();
            if (std::cin.eof()) return false;
            b = QoiReadU8();
            if (std::cin.eof()) return false;
            a = QoiReadU8();
            if (std::cin.eof()) return false;
        } else {
            uint8_t tag = op & QOI_MASK_2;
            if (tag == QOI_OP_INDEX_TAG) {
                uint8_t index = op & 0x3f;
                r = history[index][0];
                g = history[index][1];
                b = history[index][2];
                a = history[index][3];
                // For RGB images, ensure alpha is 255
                if (channels == 3) a = 255;
            } else if (tag == QOI_OP_DIFF_TAG) {
                uint8_t dr = (op >> 4) & 0x03;
                uint8_t dg = (op >> 2) & 0x03;
                uint8_t db = op & 0x03;
                r = pre_r + (dr - 2);
                g = pre_g + (dg - 2);
                b = pre_b + (db - 2);
                // a remains unchanged
                if (channels == 3) a = 255;
            } else if (tag == QOI_OP_LUMA_TAG) {
                uint8_t dg = (op & 0x3f) - 32;
                uint8_t next = QoiReadU8();
                if (std::cin.eof()) return false;
                uint8_t dr_dg = (next >> 4) & 0x0f;
                uint8_t db_dg = next & 0x0f;
                r = pre_r + dg + (dr_dg - 8);
                g = pre_g + dg;
                b = pre_b + dg + (db_dg - 8);
                // a remains unchanged
                if (channels == 3) a = 255;
            } else if (tag == QOI_OP_RUN_TAG) {
                uint8_t run = (op & 0x3f) + 1;
                // Check if run exceeds remaining pixels
                if (i + run > px_num) return false;
                for (int j = 0; j < run; j++) {
                    QoiWriteU8(pre_r);
                    QoiWriteU8(pre_g);
                    QoiWriteU8(pre_b);
                    if (channels == 4) QoiWriteU8(pre_a);
                }
                i += run - 1; // -1 because for loop will increment i
                continue;
            } else {
                // Invalid tag
                return false;
            }
        }
        
        // For RGB images, ensure alpha is always 255 before updating history
        if (channels == 3) a = 255;
        
        // Update history
        int index = QoiColorHash(r, g, b, a);
        history[index][0] = r;
        history[index][1] = g;
        history[index][2] = b;
        history[index][3] = a;
        
        // Write pixel
        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);
        
        // Update previous pixel
        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }

    bool valid = true;
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        uint8_t pad = QoiReadU8();
        if (std::cin.eof()) return false;
        if (pad != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
