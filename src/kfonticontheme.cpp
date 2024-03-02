#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_MULTIPLE_MASTERS_H

#include <QPainter>
#include <QVarLengthArray>

#include "kfonticontheme.h"
#include "kiconcolors.h"

/*

Freetype Wrappers

*/

namespace FT
{

template<typename T>
using Deleter = FT_Error (*)(FT_Library, T *);

template<typename T, Deleter<T> fn>
class Box
{
    FT_Library library;
    T *ptr = nullptr;

public:
    Box(FT_Library library)
        : library(library)
    {
    }
    ~Box()
    {
        if (ptr != nullptr) {
            fn(library, ptr);
        }
    }
    T **operator&()
    {
        return &ptr;
    }
    T *operator->()
    {
        return ptr;
    }
};

using MMVar = Box<FT_MM_Var, FT_Done_MM_Var>;

class Library
{
    FT_Library library;

public:
    Library()
    {
        auto err = FT_Init_FreeType(&library);
        Q_ASSERT(err == 0);
    };
    ~Library()
    {
        FT_Done_FreeType(library);
    }
    operator FT_Library() const
    {
        return library;
    }
    FT_Library *operator&()
    {
        return &library;
    }
};

FT::Library library;

class Face
{
    FT_Face face;
    std::unique_ptr<QFileDevice> device;

public:
    Face(FT_Face face, std::unique_ptr<QFileDevice> &&device)
        : face(face)
        , device(std::move(device))
    {
    }
    ~Face()
    {
        FT_Done_Face(face);
    }
    operator FT_Face() const
    {
        return face;
    }
    FT_Face *operator&()
    {
        return &face;
    }
    FT_Face operator->() const
    {
        return face;
    }
};

class Bitmap
{
    FT_Bitmap bitmap;
    FT_Library library;

public:
    Bitmap(FT_Library library)
        : library(library)
    {
        FT_Bitmap_Init(&bitmap);
    }
    ~Bitmap()
    {
        FT_Bitmap_Done(library, &bitmap);
    }
    operator FT_Bitmap() const
    {
        return bitmap;
    }
    FT_Bitmap *operator&()
    {
        return &bitmap;
    }
    FT_Bitmap *operator->()
    {
        return &bitmap;
    }
};

};

using Err = KFontIconTheme::FreeTypeError;

/*

Font face loading code

*/

std::variant<std::unique_ptr<KFontIconTheme>, QFileDevice::FileError, KFontIconTheme::FreeTypeError>
KFontIconTheme::initFromDevice(std::unique_ptr<QFileDevice> device)
{
    auto size = device->size();
    auto *addr = device->map(0, size, QFileDevice::MapPrivateOption);
    if (addr == nullptr) {
        return {device->error()};
    }
    FT_Face face;
    auto err = FT_New_Memory_Face(FT::library, addr, size, 0, &face);
    if (err != 0) {
        return {Err{"creating a memory face", err}};
    }
    return {std::unique_ptr<KFontIconTheme>(new KFontIconTheme(std::make_unique<FT::Face>(face, std::move(device))))};
}

/*

Constructing the actual KFontIconTheme

*/

struct KFontIconThemePrivate {
    std::unique_ptr<FT::Face> face;
};

KFontIconTheme::KFontIconTheme(std::unique_ptr<FT::Face> &&face)
    : d(new KFontIconThemePrivate{std::move(face)})
{
}

/**
 * Create a pixmap for the given icon, if present.
 */
std::variant<QPixmap, KFontIconTheme::FreeTypeError>
KFontIconTheme::createPixmapForIcon(const QString &iconName, const QSize &size, qreal scale, KIconColors iconColors, KIconLoader::States state)
{
    // set the pixel size of the face
    auto err = FT_Set_Pixel_Sizes(*d->face, size.width(), size.height());
    if (err != 0) {
        return {Err{"setting font size", err}};
    }

    // retrieve the font's variable axes (we're looking for optical size)
    FT::MMVar axisInformation(FT::library);
    err = FT_Get_MM_Var(*d->face, &axisInformation);
    if (err != 0) {
        return {Err{"obtaining list of font variable axes", err}};
    }

    std::optional<int> opticalSizeIndex;
    for (FT_UInt i = 0; i < axisInformation->num_axis; i++) {
        if (axisInformation->axis[i].tag == 'opsz') {
            opticalSizeIndex = i;
            break;
        }
    }

    // if we found the axis, let's set it
    if (opticalSizeIndex.has_value()) {
        auto idx = opticalSizeIndex.value();

        // 8 is the smallest power of two that can fit all five standard axes
        QVarLengthArray<FT_Fixed, 8> axes(axisInformation->num_axis);

        err = FT_Get_Var_Design_Coordinates(*d->face, axisInformation->num_axis, axes.data());
        if (err != 0) {
            return {Err{"obtaining font variable axis defaults", err}};
        }

        // like fonts, icons are usually more consistent in height than they are in width
        // so we pick the height as our representative optical size
        auto scaledSize = size / scale;
        // also don't forget to left shift by 15 to convert to fixed point
        axes[idx] = scaledSize.height() << 15;

        err = FT_Set_Var_Design_Coordinates(*d->face, axisInformation->num_axis, axes.data());
        if (err != 0) {
            return {Err{"modifiying font variable axes", err}};
        }
    }

    // this is the part where we handle recolouring
    FT_Palette_Data paletteData;
    err = FT_Palette_Data_Get(*d->face, &paletteData);
    if (err != 0) {
        return {Err{"obtaining information about font palette", err}};
    }

    FT_Color *palette = nullptr;
    err = FT_Palette_Select(*d->face, 0, &palette);
    if (err != 0) {
        return {Err{"obtaining default palette", err}};
    }

    // we mutate the returned array of colors to match the provided color scheme
    // (we don't need to follow this up with a setter function)
    auto colors = iconColors.colors(state);
    for (int i = 0; i < colors.length() && i < paletteData.num_palette_entries; i++) {
        palette[i].red = colors[i].red();
        palette[i].green = colors[i].green();
        palette[i].blue = colors[i].blue();
        palette[i].alpha = colors[i].alpha();
    }

    // we've done the chorework of configuring the font face, now is when we actually look up the glyph and render it
    auto idx = FT_Get_Name_Index(*d->face, qPrintable(iconName));
    if (idx == 0) {
        return {Err{"looking up icon in the font", FT_Err_Invalid_Character_Code}};
    }

    err = FT_Load_Glyph(*d->face, idx, FT_LOAD_COLOR);
    if (err != 0) {
        return {Err{"loading icon from font", err}};
    }

    err = FT_Render_Glyph((*d->face)->glyph, FT_RENDER_MODE_NORMAL);
    if (err != 0) {
        return {Err{"rendering icon", err}};
    }

    // now we get the buffer from within freetype to a QImage/QPixmap,
    // which is a somewhat involved process

    auto &bitmapInformation = (*d->face)->glyph;
    auto &bitmap = bitmapInformation->bitmap;

    auto output = &bitmap;
    QImage src;

    switch (output->pixel_mode) {
    case FT_PIXEL_MODE_GRAY: {
        // "gray" mode from freetype isn't a greyscale, but an alpha coverage map.
        // (we don't import it with QImage::Format_Greyscale8)
        // instead, FT_Bitmap_Blend with a foreground colour will do the hard work
        // of converting from the alpha coverage map into a semi-translucent bitmap
        // that we can composite as normal.
        FT::Bitmap blended(FT::library);
        auto offset = FT_Vector{0, 0};

        FT_Color fg;
        fg.red = colors[0].red();
        fg.green = colors[0].green();
        fg.blue = colors[0].blue();
        fg.alpha = colors[0].alpha();

        FT_Bitmap_Blend(FT::library, output, FT_Vector{0, 0}, &blended, &offset, fg);

        src = QImage(output->width, output->rows, QImage::Format_ARGB32_Premultiplied);

        auto ptr = blended->buffer;
        for (FT_UInt row = 0; row < blended->rows; ptr += blended->pitch, row++) {
            memcpy(src.scanLine(row), ptr, blended->width * 4);
        }

        break;
    }
    case FT_PIXEL_MODE_BGRA: {
        // if we're here that means we got a colour font, which is in ARGB32
        // (qt and freetype seem to define modes in different endianness, but their buffers are compatible)
        src = QImage(output->width, output->rows, QImage::Format_ARGB32_Premultiplied);

        auto ptr = output->buffer;
        for (FT_UInt row = 0; row < output->rows; ptr += output->pitch, row++) {
            memcpy(src.scanLine(row), ptr, output->width * 4);
        }
        break;
    }
    default: {
        // unhandled mode;
        Q_UNREACHABLE();
    }
    }

    // freetype provides an image that fits within the bounds we provided, not the whole bounds.
    // thus, it's up to us to manually place the returned image within our desired bounds.

    QPixmap out(size);
    out.fill(Qt::transparent);

    {
        QPainter painter(&out);
        painter.drawImage(QPoint(bitmapInformation->bitmap_left, size.height() - bitmapInformation->bitmap_top), src);
    }

    out.setDevicePixelRatio(scale);
    return out;
}

QString KFontIconTheme::name() const
{
    // TODO: query the SFNT table
    return {};
}

QStringList KFontIconTheme::queryIcons() const
{
    // TODO: query the list of icons
    return {};
}

bool KFontIconTheme::containsIcon(const QString &iconName) const
{
    auto idx = FT_Get_Name_Index(*d->face, qPrintable(iconName));
    return idx != 0;
}
