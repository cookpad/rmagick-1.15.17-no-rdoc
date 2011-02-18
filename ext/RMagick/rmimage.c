/* $Id: rmimage.c,v 1.192.2.5.2.9 2008/12/05 00:44:41 rmagick Exp $ */
/*============================================================================\
|                Copyright (C) 2008 by Timothy P. Hunter
| Name:     rmimage.c
| Author:   Tim Hunter
| Purpose:  Image class method definitions for RMagick
\============================================================================*/

#include "rmagick.h"

// magick_config.h in GraphicsMagick doesn't define HasX11
#if defined(HAVE_XIMPORTIMAGE)
#if !defined(HasX11)
#define HasX11
#endif
#include "magick/xwindow.h"     // XImageInfo
#endif

typedef Image *(effector_t)(const Image *, const double, const double, ExceptionInfo *);
typedef Image *(flipper_t)(const Image *, ExceptionInfo *);
typedef Image *(magnifier_t)(const Image *, ExceptionInfo *);
typedef Image *(reader_t)(const Info *, ExceptionInfo *);
typedef Image *(scaler_t)(const Image *, const unsigned long, const unsigned long, ExceptionInfo *);
typedef unsigned int (thresholder_t)(Image *, const char *);
typedef Image *(xformer_t)(const Image *, const RectangleInfo *, ExceptionInfo *);

static VALUE cropper(int, int, VALUE *, VALUE);
static VALUE effect_image(VALUE, int, VALUE *, effector_t);
static VALUE flipflop(int, VALUE, flipper_t);
static VALUE rd_image(VALUE, VALUE, reader_t);
static VALUE rotate(int, int, VALUE *, VALUE);
static VALUE scale(int, int, VALUE *, VALUE, scaler_t);
static VALUE threshold_image(int, VALUE *, VALUE, thresholder_t);
static VALUE xform_image(int, VALUE, VALUE, VALUE, VALUE, VALUE, xformer_t);
static VALUE array_from_images(Image *);

static ImageAttribute *Next_Attribute;

static const char *BlackPointCompensationKey = "PROFILE:black-point-compensation";




/*
    Static:     adaptive_method
    Purpose:    call Adaptive(Blur|Sharpen)Image
*/
#if defined(HAVE_ADAPTIVEBLURIMAGECHANNEL) || defined(HAVE_ADAPTIVESHARPENIMAGE)
static VALUE adaptive_method(
    int argc,
    VALUE *argv,
    VALUE self,
    Image *fp(const Image *, const double, const double, ExceptionInfo *))
{
    Image *image, *new_image;
    double radius = 0.0;
    double sigma = 1.0;
    ExceptionInfo exception;

    switch (argc)
    {
        case 2:
            sigma = NUM2DBL(argv[1]);
        case 1:
            radius = NUM2DBL(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 2)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);

    GetExceptionInfo(&exception);

    new_image = (fp)(image, radius, sigma, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}



/*
    Static:     adaptive_channel_method
    Purpose:    call Adaptive(Blur|Sharpen)ImageChannel
*/
static VALUE adaptive_channel_method(
    int argc,
    VALUE *argv,
    VALUE self,
    Image *fp(const Image *, const ChannelType, const double, const double, ExceptionInfo *))
{
    Image *image, *new_image;
    double radius = 0.0;
    double sigma = 1.0;
    ExceptionInfo exception;
    ChannelType channels;

    channels = extract_channels(&argc, argv);

    switch (argc)
    {
        case 2:
            sigma = NUM2DBL(argv[1]);
        case 1:
            radius = NUM2DBL(argv[0]);
        case 0:
            break;
        default:
            raise_ChannelType_error(argv[argc-1]);
            break;
    }

    Data_Get_Struct(self, Image, image);

    GetExceptionInfo(&exception);

    new_image = (fp)(image, channels, radius, sigma, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}
#endif


/*
    Method:     Image#adaptive_blur(radius=0.0, sigma=1.0)
    Purpose:    call AdaptiveBlurImage
*/
VALUE
Image_adaptive_blur(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_ADAPTIVEBLURIMAGECHANNEL)
    return adaptive_method(argc, argv, self, AdaptiveBlurImage);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}



/*
    Method:     Image#adaptive_blur_channel(radius=0.0, sigma=1.0[ , channel...])
    Purpose:    call AdaptiveBlurImageChannel
*/
VALUE
Image_adaptive_blur_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_ADAPTIVEBLURIMAGECHANNEL)
    return adaptive_channel_method(argc, argv, self, AdaptiveBlurImageChannel);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#adaptive_resize(scale)
                Image#adaptive_resize(cols, rows)
    Purpose:    Call AdaptiveResizeImage
*/
VALUE
Image_adaptive_resize(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_ADAPTIVERESIZEIMAGE)

    Image *image, *new_image;
    unsigned long rows, columns;
    double scale, drows, dcols;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    switch (argc)
    {
        case 2:
            rows = NUM2ULONG(argv[1]);
            columns = NUM2ULONG(argv[0]);
            break;
        case 1:
            scale = NUM2DBL(argv[0]);
            if (scale < 0.0)
            {
                rb_raise(rb_eArgError, "invalid scale value (%g given)", scale);
            }
            drows = scale * image->rows + 0.5;
            dcols = scale * image->columns + 0.5;
            if (drows > (double)ULONG_MAX || dcols > (double)ULONG_MAX)
            {
                rb_raise(rb_eRangeError, "resized image too big");
            }
            rows = (unsigned long) drows;
            columns = (unsigned long) dcols;
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 or 2)", argc);
            break;
    }

    GetExceptionInfo(&exception);
    new_image = AdaptiveResizeImage(image, columns, rows, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);
    rm_ensure_result(new_image);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}



/*
    Method:     Image#adaptive_sharpen(radius=0.0, sigma=1.0)
    Purpose:    call AdaptiveSharpenImage
*/
VALUE
Image_adaptive_sharpen(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_ADAPTIVESHARPENIMAGE)
    return adaptive_method(argc, argv, self, AdaptiveSharpenImage);
#else

    rm_not_implemented();
    return (VALUE)0;
#endif
}



/*
    Method:     Image#adaptive_sharpen_channel(radius=0.0, sigma=1.0[, channel...])
    Purpose:    Call AdaptiveSharpenImageChannel
*/
VALUE
Image_adaptive_sharpen_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_ADAPTIVESHARPENIMAGE)
    return adaptive_channel_method(argc, argv, self, AdaptiveSharpenImageChannel);
#else

    rm_not_implemented();
    return (VALUE)0;
#endif
}



/*
    Method:     Image#adaptive_threshold(width=3, height=3, offset=0)
    Purpose:    selects an individual threshold for each pixel based on
                the range of intensity values in its local neighborhood.
                This allows for thresholding of an image whose global
                intensity histogram doesn't contain distinctive peaks.
    Returns:    a new image
*/
VALUE
Image_adaptive_threshold(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    unsigned long width = 3, height = 3;
    long offset = 0;
    ExceptionInfo exception;

    switch (argc)
    {
        case 3:
           offset = NUM2LONG(argv[2]);
        case 2:
           height = NUM2ULONG(argv[1]);
        case 1:
           width  = NUM2ULONG(argv[0]);
        case 0:
           break;
        default:
           rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 3)", argc);
    }

    Data_Get_Struct(self, Image, image);

    GetExceptionInfo(&exception);
    new_image = AdaptiveThresholdImage(image, width, height, offset, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Method:     Image#add_noise(noise_type)
    Purpose:    add random noise to a copy of the image
    Returns:    a new image
*/
VALUE
Image_add_noise(VALUE self, VALUE noise)
{
    Image *image, *new_image;
    NoiseType noise_type;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    VALUE_TO_ENUM(noise, noise_type, NoiseType);

    GetExceptionInfo(&exception);
    new_image = AddNoiseImage(image, noise_type, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Method:     Image#add_noise_channel(noise_type[,channel...])
    Purpose:    add random noise to a copy of the image
    Returns:    a new image
*/
VALUE
Image_add_noise_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_ADDNOISEIMAGECHANNEL)
    Image *image, *new_image;
    NoiseType noise_type;
    ExceptionInfo exception;
    ChannelType channels;

    channels = extract_channels(&argc, argv);

    // There must be 1 remaining argument.
    if (argc == 0)
    {
        rb_raise(rb_eArgError, "missing noise type argument");
    }
    else if (argc > 1)
    {
        raise_ChannelType_error(argv[argc-1]);
    }

    Data_Get_Struct(self, Image, image);

    VALUE_TO_ENUM(argv[0], noise_type, NoiseType);
    channels &= ~OpacityChannel;

    GetExceptionInfo(&exception);
    new_image = AddNoiseImageChannel(image, channels, noise_type, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#add_profile(name)
    Purpose:    adds all the profiles in the specified file
    Notes:      `name' is the profile filename
*/
VALUE
Image_add_profile(VALUE self, VALUE name)
{
#if defined(HAVE_GETNEXTIMAGEPROFILE)
    // ImageMagick code based on the code for the "-profile" option in mogrify.c
    Image *image, *profile_image;
    ImageInfo *info;
    ExceptionInfo exception;
    char *profile_name;
    char *profile_filename = NULL;
    long profile_filename_l = 0;
    const StringInfo *profile;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    // ProfileImage issues a warning if something goes wrong.
    profile_filename = STRING_PTR_LEN(name, profile_filename_l);

    info = CloneImageInfo(NULL);
    info->client_data= (void *)GetImageProfile(image,"8bim");

    strncpy(info->filename, profile_filename, min((size_t)profile_filename_l, sizeof(info->filename)));
    info->filename[MaxTextExtent-1] = '\0';

    GetExceptionInfo(&exception);
    profile_image = ReadImage(info, &exception);
    (void) DestroyImageInfo(info);
    rm_check_exception(&exception, profile_image, DestroyOnError);
    (void) DestroyExceptionInfo(&exception);
    rm_ensure_result(profile_image);

    ResetImageProfileIterator(profile_image);
    profile_name = GetNextImageProfile(profile_image);
    while (profile_name)
    {
        profile = GetImageProfile(profile_image, profile_name);
        if (profile)
        {
            (void)ProfileImage(image, profile_name, profile->datum, (unsigned long)profile->length, MagickFalse);
            if (image->exception.severity >= ErrorException)
            {
                break;
            }
        }
        profile_name = GetNextImageProfile(profile_image);
    }

    (void) DestroyImage(profile_image);
    rm_check_image_exception(image, RetainOnError);

#else

    // GraphicsMagick code based on the code for the "-profile" option in command.c
    Image *image, *profile_image;
    ImageInfo *info;
    ExceptionInfo exception;
#if !defined(HAVE_IMAGE_IPTC_PROFILE)
    ProfileInfo profile_info;
#endif
    char *profile_filename = NULL;
    long profile_filename_l = 0;
    const unsigned char *profile;
    size_t profile_l;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    // ProfileImage issues a warning if something goes wrong.
    profile_filename = STRING_PTR_LEN(name, profile_filename_l);

    info = CloneImageInfo(NULL);
#if !defined(HAVE_IMAGE_IPTC_PROFILE)
    profile_info.name="IPTC";
    profile_info.info=(unsigned char *) GetImageProfile(image, profile_info.name, &profile_info.length);
    info->client_data=&profile_info;
#else
    info->client_data= (void *) &image->iptc_profile;
#endif
    strncpy(info->filename, profile_filename, min(profile_filename_l, (long)sizeof(info->filename)));
    info->filename[MaxTextExtent-1] = '\0';

    GetExceptionInfo(&exception);
    profile_image = ReadImage(info, &exception);
    (void) DestroyImageInfo(info);
    rm_check_exception(&exception, profile_image, DestroyOnError);
    (void) DestroyExceptionInfo(&exception);
    rm_ensure_result(profile_image);

    /* IPTC NewsPhoto Profile */
    profile = GetImageProfile(profile_image, "IPTC", &profile_l);
    if (profile)
    {
        (void)SetImageProfile(image, "IPTC", profile, profile_l);
        if (image->exception.severity >= ErrorException)
        {
            goto done;
        }
    }

#if defined(HAVE_ALLOCATEIMAGEPROFILEITERATOR)
    /* GraphicsMagick 1.2 */
    {
    ImageProfileIterator profile_iterator;
    const char *profile_name;
    size_t profile_length;

    profile_iterator = AllocateImageProfileIterator(profile_image);
    while (NextImageProfile(profile_iterator, &profile_name, &profile, &profile_length) != MagickFail)
      {
        if ((rm_strcasecmp(profile_name, "ICC") == 0) || (rm_strcasecmp(profile_name, "ICM") == 0))
          (void) ProfileImage(image, profile_name, (unsigned char *) profile, profile_length, True);
        else
          (void) SetImageProfile(image, profile_name, profile, profile_length);
      }
    DeallocateImageProfileIterator(profile_iterator);
    }

#else
    {
    long x;
    ProfileInfo *generic;

    /* ICC ICM Profile */
    profile = GetImageProfile(profile_image, "ICM", &profile_l);
    if (profile)
    {
        (void)SetImageProfile(image, "ICM", profile, profile_l);
        if (image->exception.severity >= ErrorException)
        {
            goto done;
        }
    }

    /* Generic Profiles */
    for (x = 0; x < (long)profile_image->generic_profiles; x++)
    {
        generic = profile_image->generic_profile + x;
        (void)SetImageProfile(image, generic->name, generic->info, generic->length);
        if (image->exception.severity >= ErrorException)
        {
            break;
        }
    }
    }
#endif

done:
    (void) DestroyImage(profile_image);
    rm_check_image_exception(image, RetainOnError);

#endif

    return self;
}


/*
    Method:     Image#affine_transform(affine_matrix)
    Purpose:    transforms an image as dictated by the affine matrix argument
    Returns:    a new image
*/
VALUE
Image_affine_transform(VALUE self, VALUE affine)
{
    Image *image, *new_image;
    ExceptionInfo exception;
    AffineMatrix matrix;

    Data_Get_Struct(self, Image, image);

    // Convert Magick::AffineMatrix to AffineMatrix structure.
    AffineMatrix_to_AffineMatrix(&matrix, affine);

    GetExceptionInfo(&exception);
    new_image = AffineTransformImage(image, &matrix, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Method:     Image#["key"]
                Image#[:key]
    Purpose:    Return the image property associated with "key"
    Returns:    property value or nil if key doesn't exist
    Notes:      Use Image#[]= (aset) to establish more properties
                or change the value of an existing property.
*/
VALUE
Image_aref(VALUE self, VALUE key_arg)
{
    Image *image;
    char *key;
    const char *attr;

    switch (TYPE(key_arg))
    {
        case T_NIL:
            return Qnil;

        case T_SYMBOL:
            key = rb_id2name((ID)SYM2ID(key_arg));
            break;

        default:
            key = STRING_PTR(key_arg);
            if (*key == '\0')
            {
                return Qnil;
            }
            break;
    }

    Data_Get_Struct(self, Image, image);

    if (rm_strcasecmp(key, "EXIF:*") == 0)
    {
        return rm_exif_by_entry(image);
    }
    else if (rm_strcasecmp(key, "EXIF:!") == 0)
    {
        return rm_exif_by_number(image);
    }

    attr = rm_get_property(image, key);
    return attr ? rb_str_new2(attr) : Qnil;
}

/*
    Method:     Image#["key"] = attr
                Image#[:key] = attr
    Purpose:    Update or add image attribute "key"
    Returns:    self
    Notes:      Specify attr=nil to remove the key from the list.

                SetImageAttribute normally APPENDS the new value
                to any existing value. Since this usage is tremendously
                counter-intuitive, this function always deletes the
                existing value before setting the new value.

                There's no use checking the return value since
                SetImageAttribute returns "False" for many reasons,
                some legitimate.
*/
VALUE
Image_aset(VALUE self, VALUE key_arg, VALUE attr_arg)
{
    Image *image;
    char *key, *attr;
    unsigned int okay;

    rm_check_frozen(self);

    attr = attr_arg == Qnil ? NULL : STRING_PTR(attr_arg);

    switch (TYPE(key_arg))
    {
        case T_NIL:
            return self;

        case T_SYMBOL:
            key = rb_id2name((ID)SYM2ID(key_arg));
            break;

        default:
            key = STRING_PTR(key_arg);
            if (*key == '\0')
            {
                return self;
            }
            break;
    }

    Data_Get_Struct(self, Image, image);

    // If we're currently looping over the attributes via
    // Image_properties (below) then check to see if we're
    // about to delete the next attribute. If so, change
    // the "next" pointer to point to the attribute following
    // this one. (No, this isn't thread-safe!)

#if !defined(HAVE_GETIMAGEPROPERTY) && !defined(HAVE_GETNEXTIMAGEATTRIBUTE)
    if (Next_Attribute)
    {
        const ImageAttribute *attribute = GetImageAttribute(image, key);
        if (attribute && attribute == Next_Attribute)
        {
            Next_Attribute = attribute->next;
        }
    }
#endif

    // Delete existing value. SetImageAttribute returns False if
    // the attribute doesn't exist - we don't care.
    (void) rm_set_property(image, key, NULL);
    // Set new value
    if (attr)
    {
        okay = rm_set_property(image, key, attr);
        if (!okay)
        {
            rb_warning("SetImageAttribute failed (probably out of memory)");
        }
    }
    return self;
}


/*
    Static:     crisscross
    Purpose:    Handle #transverse, #transform methods
*/
#if defined(HAVE_TRANSPOSEIMAGE) || defined(HAVE_TRANSVERSEIMAGE)
static VALUE
crisscross(
    int bang,
    VALUE self,
    Image *fp(const Image *, ExceptionInfo *))
{
    Image *image, *new_image;
    ExceptionInfo exception;


    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = (fp)(image, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    if (bang)
    {
        DATA_PTR(self) = new_image;
        (void) DestroyImage(image);
        return self;
    }

    return rm_image_new(new_image);

}
#endif


/*
    Method:     Image#auto_orient
    Purpose:    Implement mogrify's -auto_orient option
                automatically orient image based on EXIF orientation value
    Notes:      See mogrify.c in ImageMagick 6.2.8.
*/
static VALUE auto_orient(int bang, VALUE self)
{
#if defined(HAVE_TRANSPOSEIMAGE) || defined(HAVE_TRANSVERSEIMAGE)
    Image *image;
    volatile VALUE new_image;
    VALUE degrees[1];

    Data_Get_Struct(self, Image, image);

    switch (image->orientation)
    {
        case TopRightOrientation:
            new_image = flipflop(bang, self, FlopImage);
            break;

        case BottomRightOrientation:
            degrees[0] = rb_float_new(180.0);
            new_image = rotate(bang, 1, degrees, self);
            break;

        case BottomLeftOrientation:
            new_image = flipflop(bang, self, FlipImage);
            break;

        case LeftTopOrientation:
            new_image = crisscross(bang, self, TransposeImage);
            break;

        case RightTopOrientation:
            degrees[0] = rb_float_new(90.0);
            new_image = rotate(bang, 1, degrees, self);
            break;

        case RightBottomOrientation:
            new_image = crisscross(bang, self, TransverseImage);
            break;

        case LeftBottomOrientation:
            degrees[0] = rb_float_new(270.0);
            new_image = rotate(bang, 1, degrees, self);
            break;

        default:                // Return IMMEDIATELY
            return bang ? Qnil : Image_copy(self);
            break;
    }


    Data_Get_Struct(new_image, Image, image);
    image->orientation = TopLeftOrientation;

    return new_image;

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


VALUE
Image_auto_orient(VALUE self)
{
    return auto_orient(False, self);
}

/*
    Returns nil if the image is already properly oriented
*/
VALUE
Image_auto_orient_bang(VALUE self)
{
    rm_check_frozen(self);
    return auto_orient(True, self);
}


/*
    Method:     Image#background_color
    Purpose:    Return the name of the background color as a String.
*/
VALUE
Image_background_color(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return PixelPacket_to_Color_Name(image, &image->background_color);
}


/*
    Method:     Image#background_color=
    Purpose:    Set the the background color to the specified color spec.
*/
VALUE
Image_background_color_eq(VALUE self, VALUE color)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    Color_to_PixelPacket(&image->background_color, color);
    return self;
}


/*
    Method:     Image#base_columns
    Purpose:    Return the number of rows (before transformations)
*/
VALUE Image_base_columns(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return INT2FIX(image->magick_columns);
}

/*
    Method:     Image#base_filename
    Purpose:    Return the image filename (before transformations)
    Notes:      If there is no base filename, return the current filename.
*/
VALUE Image_base_filename(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    if (*image->magick_filename)
    {
        return rb_str_new2(image->magick_filename);
    }
    else
    {
        return rb_str_new2(image->filename);
    }
}

/*
    Method:     Image#base_rows
    Purpose:    Return the number of rows (before transformations)
*/
VALUE Image_base_rows(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return INT2FIX(image->magick_rows);
}


/*
    Method:     Image#bias  -> bias
                Image#bias = a number between 0.0 and 1.0 or "NN%"
    Purpose:    Get/set image bias (used when convolving an image)
*/
VALUE Image_bias(VALUE self)
{
#if defined(HAVE_IMAGE_BIAS)
    Image *image;

    Data_Get_Struct(self, Image, image);
    return rb_float_new(image->bias);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


VALUE Image_bias_eq(VALUE self, VALUE pct)
{
#if defined(HAVE_IMAGE_BIAS)
    Image *image;
    double bias;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    bias = rm_percentage(pct);
    image->bias = bias * MaxRGB;

    return self;

#else
    rm_not_implemented();
    return (VALUE)0;
#endif

}

/*
 *  Method:     Image#bilevel_channel(threshold, channel=AllChannels)
 *  Returns     a new image
*/
VALUE
Image_bilevel_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_BILEVELIMAGECHANNEL)
    Image *image, *new_image;
    ChannelType channels;

    channels = extract_channels(&argc, argv);

    if (argc > 1)
    {
        raise_ChannelType_error(argv[argc-1]);
    }
    if (argc == 0)
    {
        rb_raise(rb_eArgError, "no threshold specified");
    }

    Data_Get_Struct(self, Image, image);
    new_image = rm_clone_image(image);

    (void)BilevelImageChannel(new_image, channels, NUM2DBL(argv[0]));
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#black_point_compensation
    Purpose:    Return current value
*/
VALUE
Image_black_point_compensation(VALUE self)
{
    Image *image;
    const ImageAttribute *attr;
    volatile VALUE value;

    Data_Get_Struct(self, Image, image);

    attr = GetImageAttribute(image, BlackPointCompensationKey);
    if (attr && rm_strcasecmp(attr->value, "true") == 0)
    {
        value = Qtrue;
    }
    else
    {
        value = Qfalse;
    }
    return value;
}


/*
    Method:     Image#black_point_compensation=true or false
    Purpose:    Set black point compensation attribute
*/
VALUE
Image_black_point_compensation_eq(VALUE self, VALUE arg)
{
    Image *image;
    char *value;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    (void) SetImageAttribute(image, BlackPointCompensationKey, NULL);
    value = RTEST(arg) ? "true" : "false";
    (void) SetImageAttribute(image, BlackPointCompensationKey, value);

    return self;
}


/*
 *  Method:     Image#black_threshold(red_channel [, green_channel
 *                                    [, blue_channel [, opacity_channel]]]);
 *  Purpose:    Call BlackThresholdImage
*/
VALUE
Image_black_threshold(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_BLACKTHRESHOLDIMAGE)
    return threshold_image(argc, argv, self, BlackThresholdImage);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Static:     get_relative_offsets
    Purpose:    compute offsets using the gravity to determine what the
                offsets are relative to
*/
static void
get_relative_offsets(
    VALUE grav,
    Image *image,
    Image *mark,
    long *x_offset,
    long *y_offset)
{
    MagickEnum *magick_enum;
    GravityType gravity;

    VALUE_TO_ENUM(grav, gravity, GravityType);

    switch(gravity)
    {
        case NorthEastGravity:
        case EastGravity:
        case SouthEastGravity:
            *x_offset = (long)(image->columns) - (long)(mark->columns) - *x_offset;
            break;
        case NorthGravity:
        case SouthGravity:
        case CenterGravity:
        case StaticGravity:
            *x_offset += (long)(image->columns/2) - (long)(mark->columns/2);
            break;
        default:
            break;
    }
    switch(gravity)
    {
        case SouthWestGravity:
        case SouthGravity:
        case SouthEastGravity:
            *y_offset = (long)(image->rows) - (long)(mark->rows) - *y_offset;
            break;
        case EastGravity:
        case WestGravity:
        case CenterGravity:
        case StaticGravity:
            *y_offset += (long)(image->rows/2) - (long)(mark->rows/2);
            break;
        case NorthEastGravity:
        case NorthGravity:
            // Don't let these run into the default case
            break;
        default:
            Data_Get_Struct(grav, MagickEnum, magick_enum);
            rb_warning("gravity type `%s' has no effect", rb_id2name(magick_enum->id));
            break;
    }

}


/*
    Static:     get_offsets_from_gravity
    Purpose:    compute watermark offsets from gravity type
*/
static void
get_offsets_from_gravity(
    GravityType gravity,
    Image *image,
    Image *mark,
    long *x_offset,
    long *y_offset)
{

    switch (gravity)
    {
        case ForgetGravity:
        case NorthWestGravity:
            *x_offset = 0;
            *y_offset = 0;
        break;
        case NorthGravity:
            *x_offset = ((long)(image->columns) - (long)(mark->columns)) / 2;
            *y_offset = 0;
        break;
        case NorthEastGravity:
            *x_offset = (long)(image->columns) - (long)(mark->columns);
            *y_offset = 0;
        break;
        case WestGravity:
            *x_offset = 0;
            *y_offset = ((long)(image->rows) - (long)(mark->rows)) / 2;
        break;
        case StaticGravity:
        case CenterGravity:
        default:
            *x_offset = ((long)(image->columns) - (long)(mark->columns)) / 2;
            *y_offset = ((long)(image->rows) - (long)(mark->rows)) / 2;
        break;
        case EastGravity:
            *x_offset = (long)(image->columns) - (long)(mark->columns);
            *y_offset = ((long)(image->rows) - (long)(mark->rows)) / 2;
        break;
        case SouthWestGravity:
            *x_offset = 0;
            *y_offset = (long)(image->rows) - (long)(mark->rows);
        break;
        case SouthGravity:
            *x_offset = ((long)(image->columns) - (long)(mark->columns)) / 2;
            *y_offset = (long)(image->rows) - (long)(mark->rows);
        break;
        case SouthEastGravity:
            *x_offset = (long)(image->columns) - (long)(mark->columns);
            *y_offset = (long)(image->rows) - (long)(mark->rows);
        break;
    }
}


/*
    Static:     check_for_long_value
    Purpose:    called from rb_protect, returns the number if obj is really
                a numeric value.
*/
static VALUE check_for_long_value(VALUE obj)
{
    long t;
    t = NUM2LONG(obj);
    t = t;      // placate gcc
    return (VALUE)0;
}


/*
    Static:     get_composite_offsets
    Purpose:    compute x- and y-offset of source image for a compositing method
*/
static void get_composite_offsets(
    int argc,
    VALUE *argv,
    Image *dest,
    Image *src,
    long *x_offset,
    long *y_offset)
{
    GravityType gravity;
    int exc = 0;

    if (CLASS_OF(argv[0]) == Class_GravityType)
    {
        VALUE_TO_ENUM(argv[0], gravity, GravityType);

        switch (argc)
        {
            // Gravity + offset(s). Offsets are relative to the image edges
            // as specified by the gravity.
            case 3:
                *y_offset = NUM2LONG(argv[2]);
            case 2:
                *x_offset = NUM2LONG(argv[1]);
                get_relative_offsets(argv[0], dest, src, x_offset, y_offset);
                break;
            case 1:
                // No offsets specified. Compute offset based on the gravity alone.
                get_offsets_from_gravity(gravity, dest, src, x_offset, y_offset);
                break;
        }
    }
    // Gravity not specified at all. Offsets are measured from the
    // NorthWest corner. The arguments must be numbers.
    else
    {
        (void)rb_protect(check_for_long_value, argv[0], &exc);
        if (exc)
        {
            rb_raise(rb_eArgError, "expected GravityType, got %s"
                   , rb_class2name(CLASS_OF(argv[0])));
        }
        *x_offset = NUM2LONG(argv[0]);
        if (argc > 1)
        {
            *y_offset = NUM2LONG(argv[1]);
        }
    }

}


/*
    Static:     blend_geometry
    Purpose:    Convert 2 doubles to a blend or dissolve geometry string.
    Notes:      the geometry buffer needs to be at least 16 characters long.
                For safety's sake this function asserts that it is at least
                20 characters long.
                The percentages must be in the range -1000 < n < 1000. This
                is far in excess of what xMagick will allow.
*/
static void
blend_geometry(
    char *geometry,
    size_t geometry_l,
    double src_percent,
    double dst_percent)
{
    size_t sz = 0;
    int fw, prec;

    if (fabs(src_percent) >= 1000.0 || fabs(dst_percent) >= 1000.0)
    {
        if (fabs(src_percent) < 1000.0)
        {
            src_percent = dst_percent;
        }
        rb_raise(rb_eArgError, "%g is out of range +/-999.99", src_percent);
    }

    assert(geometry_l >= 20);
    memset(geometry, 0xdf, geometry_l);

    fw = 4;
    prec = 0;
    if (src_percent != floor(src_percent))
    {
        prec = 2;
        fw += 3;
    }

    sz = (size_t)sprintf(geometry, "%*.*f", -fw, prec, src_percent);
    assert(sz < geometry_l);

    sz = strcspn(geometry, " ");

    // if dst_percent was nil don't add to the geometry
    if (dst_percent != -1.0)
    {
        fw = 4;
        prec = 0;
        if (dst_percent != floor(dst_percent))
        {
            prec = 2;
            fw += 3;
        }


        sz += (size_t)sprintf(geometry+sz, "x%*.*f", -fw, prec, dst_percent);
        assert(sz < geometry_l);
        sz = strcspn(geometry, " ");
    }

    if (sz < geometry_l)
    {
        memset(geometry+sz, 0x00, geometry_l-sz);
    }

}


static VALUE
special_composite(
    Image *image,
    Image *overlay,
    double image_pct,
    double overlay_pct,
    long x_off,
    long y_off,
    CompositeOperator op)
{
    Image *new_image;
    char geometry[20];

    blend_geometry(geometry, sizeof(geometry), image_pct, overlay_pct);
    (void) CloneString(&overlay->geometry, geometry);

    new_image = rm_clone_image(image);
    (void) CompositeImage(new_image, op, overlay, x_off, y_off);

    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}


/*
    Method:     Image#blend(overlay, src_percent, dst_percent, x_offset=0, y_offset=0)
                Image#dissolve(overlay, src_percent, dst_percent, gravity, x_offset=0, y_offset=0)
    Purpose:    Corresponds to the composite -blend operation
    Notes:      `percent' can be a number or a string in the form "NN%"
                The default value for dst_percent is 100.0-src_percent
*/
VALUE
Image_blend(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_COLORDODGECOMPOSITEOP)
    Image *image, *overlay;
    double src_percent, dst_percent;
    long x_offset = 0L, y_offset = 0L;

    Data_Get_Struct(self, Image, image);

    if (argc < 1)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 to 6)", argc);
    }

    if (argc > 3)
    {
        Data_Get_Struct(ImageList_cur_image(argv[0]), Image, overlay);
        get_composite_offsets(argc-3, &argv[3], image, overlay, &x_offset, &y_offset);
        // There must be 3 arguments left
        argc = 3;
    }

    switch (argc)
    {
        case 3:
            dst_percent = rm_percentage(argv[2]) * 100.0;
            src_percent = rm_percentage(argv[1]) * 100.0;
            break;
        case 2:
            src_percent = rm_percentage(argv[1]) * 100.0;
            dst_percent = FMAX(100.0 - src_percent, 0);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 to 6)", argc);
            break;
    }

    Data_Get_Struct(ImageList_cur_image(argv[0]), Image, overlay);

    return special_composite(image, overlay, src_percent, dst_percent
                           , x_offset, y_offset, BlendCompositeOp);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


DEF_ATTR_ACCESSOR(Image, blur, dbl)


/*
 *  Method:     Image#blur_channel(radius = 0.0, sigma = 1.0, channel=AllChannels)
 *  Purpose:    Call BlurImageChannel
*/
VALUE
Image_blur_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_BLURIMAGECHANNEL)
     Image *image, *new_image;
     ExceptionInfo exception;
     ChannelType channels;
     double radius = 0.0, sigma = 1.0;

     Data_Get_Struct(self, Image, image);

     channels = extract_channels(&argc, argv);

     // There can be 0, 1, or 2 remaining arguments.
     switch (argc)
     {
         case 2:
             sigma = NUM2DBL(argv[1]);
         case 1:
             radius = NUM2DBL(argv[0]);
         case 0:
             break;
         default:
             raise_ChannelType_error(argv[argc-1]);
     }

     GetExceptionInfo(&exception);
     new_image = BlurImageChannel(image, channels, radius, sigma, &exception);
     rm_check_exception(&exception, new_image, DestroyOnError);

     (void) DestroyExceptionInfo(&exception);

     rm_ensure_result(new_image);

     return rm_image_new(new_image);
#else
     rm_not_implemented();
     return (VALUE)0;
#endif
}


/*
    Method:     Image#blur_image(radius=0.0, sigma=1.0)
    Purpose:    Blur the image
    Notes:      The "blur" name is used for the attribute
*/
VALUE
Image_blur_image(int argc, VALUE *argv, VALUE self)
{
    return effect_image(self, argc, argv, BlurImage);
}


/*
    Method:     Image#border(width, height, color)
                Image#border!(width, height, color)
    Purpose:    surrounds the image with a border of the specified width,
                height, and named color
*/
static VALUE border(
    int bang,
    VALUE self,
    VALUE width,
    VALUE height,
    VALUE color)
{
    Image *image, *new_image;
    PixelPacket old_border;
    ExceptionInfo exception;
    RectangleInfo rect = {0};

    Data_Get_Struct(self, Image, image);

    rect.width = NUM2UINT(width);
    rect.height = NUM2UINT(height);

    // Save current border color - we'll want to restore it afterwards.
    old_border = image->border_color;
    Color_to_PixelPacket(&image->border_color, color);

    GetExceptionInfo(&exception);
    new_image = BorderImage(image, &rect, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    if (bang)
    {
        new_image->border_color = old_border;
        DATA_PTR(self) = new_image;
        (void) DestroyImage(image);
        return self;
    }

    image->border_color = old_border;
    return rm_image_new(new_image);
}

VALUE
Image_border_bang(
    VALUE self,
    VALUE width,
    VALUE height,
    VALUE color)
{
    rm_check_frozen(self);
    return border(True, self, width, height, color);
}


VALUE
Image_border(
    VALUE self,
    VALUE width,
    VALUE height,
    VALUE color)
{
    return border(False, self, width, height, color);
}


/*
    Method:     Image#border_color
    Purpose:    Return the name of the border color as a String.
*/
VALUE
Image_border_color(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return PixelPacket_to_Color_Name(image, &image->border_color);
}

/*
    Method:     Image#border_color=
    Purpose:    Set the the border color
*/
VALUE
Image_border_color_eq(VALUE self, VALUE color)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    Color_to_PixelPacket(&image->border_color, color);
    return self;
}


/*
    Method:     Image#bounding_box
    Purpose:    returns the bounding box of an image canvas
*/
VALUE Image_bounding_box(VALUE self)
{
    Image *image;
    RectangleInfo box;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);
    box = GetImageBoundingBox(image, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return Rectangle_from_RectangleInfo(&box);
}


/*
    Method:     Image.capture(silent=false,
                              frame=false,
                              descend=false,
                              screen=false,
                              borders=false) { optional parms }
    Purpose:    do a screen capture
*/
VALUE
Image_capture(
    int argc,
    VALUE *argv,
    VALUE self)
{
#ifdef HAVE_XIMPORTIMAGE
    Image *image;
    ImageInfo *image_info;
    volatile VALUE info_obj;
    XImportInfo ximage_info;

    self = self;  // Suppress "never referenced" message from icc

    XGetImportInfo(&ximage_info);
    switch (argc)
    {
        case 5:
           ximage_info.borders = (MagickBooleanType)RTEST(argv[4]);
        case 4:
           ximage_info.screen  = (MagickBooleanType)RTEST(argv[3]);
        case 3:
           ximage_info.descend = (MagickBooleanType)RTEST(argv[2]);
        case 2:
           ximage_info.frame   = (MagickBooleanType)RTEST(argv[1]);
        case 1:
           ximage_info.silent  = (MagickBooleanType)RTEST(argv[0]);
        case 0:
           break;
        default:
           rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 5)", argc);
           break;
    }

    // Get optional parms.
    // Set info->filename = "root", window ID number or window name,
    //  or nothing to do an interactive capture
    // Set info->server_name to the server name
    // Also info->colorspace, depth, dither, interlace, type
    info_obj = rm_info_new();
    Data_Get_Struct(info_obj, Info, image_info);

    // If an error occurs, IM will call our error handler and we raise an exception.
    image = XImportImage(image_info, &ximage_info);
    rm_check_image_exception(image, DestroyOnError);
    rm_ensure_result(image);

    return rm_image_new(image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#change_geometry(geometry_string) { |cols, rows, image| }
    Purpose:    parse geometry string, compute new image geometry
*/
VALUE
Image_change_geometry(VALUE self, VALUE geom_arg)
{
#if defined(HAVE_PARSESIZEGEOMETRY)
    Image *image;
    RectangleInfo rect = {0};
    volatile VALUE geom_str;
    char *geometry;
    unsigned int flags;
    volatile VALUE ary;

    Data_Get_Struct(self, Image, image);
    geom_str = rb_funcall(geom_arg, rm_ID_to_s, 0);
    geometry = STRING_PTR(geom_str);

#if defined(HAVE_NEW_PARSESIZEGEOMETRY)
    flags = ParseSizeGeometry(image, geometry, &rect, &image->exception);
#else
    flags = ParseSizeGeometry(image, geometry, &rect);
#endif
    rm_check_image_exception(image, RetainOnError);

    if (flags == NoValue)
    {
       rb_raise(rb_eArgError, "invalid geometry string `%s'", geometry);
    }

    ary = rb_ary_new2(3);
    rb_ary_store(ary, 0, ULONG2NUM(rect.width));
    rb_ary_store(ary, 1, ULONG2NUM(rect.height));
    rb_ary_store(ary, 2, self);

    return rb_yield(ary);

#else
    Image *image;
    char *geometry;
    unsigned int flags;
    long x, y;
    unsigned long width, height;
    volatile VALUE ary;
    volatile VALUE geom_str;

    Data_Get_Struct(self, Image, image);
    geom_str = rb_funcall(geom_arg, rm_ID_to_s, 0);
    geometry = STRING_PTR(geom_str);

    width = image->columns;
    height = image->rows;

    flags = GetMagickGeometry(geometry, &x, &y, &width, &height);
    if (flags == NoValue)
    {
       rb_raise(rb_eArgError, "invalid geometry string `%s'", geometry);
    }

    ary = rb_ary_new2(3);
    rb_ary_store(ary, 0, ULONG2NUM(width));
    rb_ary_store(ary, 1, ULONG2NUM(height));
    rb_ary_store(ary, 2, self);

    return rb_yield(ary);

#endif
}


/*
    Method:     Image#changed?
    Purpose:    Return true if any pixel in the image has been altered since
                the image was constituted.
*/
VALUE
Image_changed_q(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return IsTaintImage(image) ? Qtrue : Qfalse;
}


/*
    Method:     Image#channel
    Purpose:    Extract a channel from the image.  A channel is a particular color
                component of each pixel in the image.
*/
VALUE
Image_channel(VALUE self, VALUE channel_arg)
{
    Image *image, *new_image;
    ChannelType channel;

    Data_Get_Struct(self, Image, image);

    VALUE_TO_ENUM(channel_arg, channel, ChannelType);

    new_image = rm_clone_image(image);

#if defined(HAVE_SEPARATEIMAGECHANNEL)
    (void) SeparateImageChannel(new_image, channel);
#else
    (void) ChannelImage(new_image, channel);
#endif

    rm_check_image_exception(new_image, DestroyOnError);
    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}


/*
    Method:     Image#channel_depth(channel_depth=AllChannels)
    Purpose:    GetImageChannelDepth
*/
VALUE
Image_channel_depth(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_GETIMAGECHANNELDEPTH)
    Image *image;
    ChannelType channels;
    unsigned long channel_depth;
    ExceptionInfo exception;

    channels = extract_channels(&argc, argv);

    // Ensure all arguments consumed.
    if (argc > 0)
    {
        raise_ChannelType_error(argv[argc-1]);
    }
    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    channel_depth = GetImageChannelDepth(image, channels, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return ULONG2NUM(channel_depth);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#channel_extrema(channel=AllChannels)
    Purpose:    Returns an array [min, max] where 'min' and 'max'
                are the minimum and maximum values of all channels.
    Notes:      GM's implementation is very different from ImageMagick.
                This method follows the IM API very closely and then
                shoehorn's the GM API to more-or-less fit. Note that
                IM allows you to specify more than one channel argument.
                GM does not.
*/
VALUE
Image_channel_extrema(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_GETIMAGECHANNELEXTREMA)    // ImageMagick 6.0.0
    Image *image;
    ChannelType channels;
    ExceptionInfo exception;
    unsigned long min, max;
    volatile VALUE ary;

    Data_Get_Struct(self, Image, image);

    channels = extract_channels(&argc, argv);

    // Ensure all arguments consumed.
    if (argc > 0)
    {
        raise_ChannelType_error(argv[argc-1]);
    }

    GetExceptionInfo(&exception);
    (void) GetImageChannelExtrema(image, channels, &min, &max, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    ary = rb_ary_new2(2);
    rb_ary_store(ary, 0, ULONG2NUM(min));
    rb_ary_store(ary, 1, ULONG2NUM(max));

    return ary;

#elif defined(HAVE_GETIMAGESTATISTICS)  // GraphicsMagick 1.1
    Image *image;
    ChannelType channel;
    ImageStatistics stats;
    ExceptionInfo exception;
    volatile VALUE ary;
    volatile VALUE type_name;

    if (argc == 0)
    {
        rb_raise(rb_eArgError, "GraphicsMagick requires at least one channel argument.");
    }
    else if (argc > 1)
    {
        rb_raise(rb_eArgError, "GraphicsMagick does not support multi-channel statistics."
                               " Specify only 1 channel.");
    }
    VALUE_TO_ENUM(argv[0], channel, ChannelType);
    if (channel == AllChannels)
    {
        rb_raise(rb_eArgError, "GraphicsMagick does not support multi-channel statistics."
                               " Specify only 1 channel.");
    }

    Data_Get_Struct(self, Image, image);

    GetExceptionInfo(&exception);

    (void) GetImageStatistics(image, &stats, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    ary = rb_ary_new2(2);
    switch(channel)
    {
        case RedChannel:
        case CyanChannel:
            rb_ary_store(ary, 0, ULONG2NUM((unsigned long)(MaxRGB*stats.red.minimum)));
            rb_ary_store(ary, 1, ULONG2NUM((unsigned long)(MaxRGB*stats.red.maximum)));
            break;
        case GreenChannel:
        case MagentaChannel:
            rb_ary_store(ary, 0, ULONG2NUM((unsigned long)(MaxRGB*stats.green.minimum)));
            rb_ary_store(ary, 1, ULONG2NUM((unsigned long)(MaxRGB*stats.green.maximum)));
            break;
        case BlueChannel:
        case YellowChannel:
            rb_ary_store(ary, 0, ULONG2NUM((unsigned long)(MaxRGB*stats.blue.minimum)));
            rb_ary_store(ary, 1, ULONG2NUM((unsigned long)(MaxRGB*stats.blue.maximum)));
            break;
        case OpacityChannel:
        case BlackChannel:
        case MatteChannel:
            rb_ary_store(ary, 0, ULONG2NUM((unsigned long)(MaxRGB*stats.opacity.minimum)));
            rb_ary_store(ary, 1, ULONG2NUM((unsigned long)(MaxRGB*stats.opacity.maximum)));
            break;
        default:
            type_name = Enum_to_s(argv[0]);
            rb_raise(rb_eArgError, "unsupported channel type: %s",
                    STRING_PTR(type_name));
    }

    return ary;

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
 *  Method:     Image#channel_mean(channel=AllChannels)
 *  Returns     An array [mean, std. deviation]
*/
VALUE
Image_channel_mean(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_GETIMAGECHANNELMEAN)   // ImageMagick 6.0.0
    Image *image;
    ChannelType channels;
    ExceptionInfo exception;
    double mean, stddev;
    volatile VALUE ary;

    Data_Get_Struct(self, Image, image);

    channels = extract_channels(&argc, argv);

    // Ensure all arguments consumed.
    if (argc > 0)
    {
        raise_ChannelType_error(argv[argc-1]);
    }

    GetExceptionInfo(&exception);
    (void) GetImageChannelMean(image, channels, &mean, &stddev, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    ary = rb_ary_new2(2);
    rb_ary_store(ary, 0, rb_float_new(mean));
    rb_ary_store(ary, 1, rb_float_new(stddev));

    return ary;

#elif defined(HAVE_GETIMAGESTATISTICS)  // GraphicsMagick 1.1
    Image *image;
    ChannelType channel;
    ImageStatistics stats;
    ExceptionInfo exception;
    volatile VALUE ary;
    volatile VALUE type_name;

    if (argc == 0)
    {
        rb_raise(rb_eArgError, "GraphicsMagick requires at least one channel argument.");
    }
    else if (argc > 1)
    {
        rb_raise(rb_eArgError, "GraphicsMagick does not support multi-channel statistics."
                               " Specify only 1 channel.");
    }
    VALUE_TO_ENUM(argv[0], channel, ChannelType);
    if (channel == AllChannels)
    {
        rb_raise(rb_eArgError, "GraphicsMagick does not support multi-channel statistics."
                               " Specify only 1 channel.");
    }

    Data_Get_Struct(self, Image, image);

    GetExceptionInfo(&exception);

    (void) GetImageStatistics(image, &stats, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    ary = rb_ary_new2(2);
    switch(channel)
    {
        case RedChannel:
        case CyanChannel:
            rb_ary_store(ary, 0, rb_float_new(stats.red.mean));
            rb_ary_store(ary, 1, rb_float_new(stats.red.standard_deviation));
            break;
        case GreenChannel:
        case MagentaChannel:
            rb_ary_store(ary, 0, rb_float_new(stats.green.mean));
            rb_ary_store(ary, 1, rb_float_new(stats.green.standard_deviation));
            break;
        case BlueChannel:
        case YellowChannel:
            rb_ary_store(ary, 0, rb_float_new(stats.blue.mean));
            rb_ary_store(ary, 1, rb_float_new(stats.blue.standard_deviation));
            break;
        case OpacityChannel:
        case BlackChannel:
        case MatteChannel:
            rb_ary_store(ary, 0, rb_float_new(stats.opacity.mean));
            rb_ary_store(ary, 1, rb_float_new(stats.opacity.standard_deviation));
            break;
        default:
            type_name = Enum_to_s(argv[0]);
            rb_raise(rb_eArgError, "unsupported channel type: %s",
                    STRING_PTR(type_name));
    }

    return ary;


#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}



/*
    Method:     Image#channel_threshold(red_channel, green_channel=MaxRGB,
                                        blue_channel=MaxRGB, opacity_channel=MaxRGB)
    Purpose:    Same as Image#threshold except that you can specify
                a separate threshold for each channel
*/
VALUE
Image_channel_threshold(int argc, VALUE *argv, VALUE self)
{
    rb_warning("This method is deprecated in this release of " Q(MAGICKNAME)
               ". Use bilevel_channel instead.");
    return threshold_image(argc, argv, self,
#if defined(HAVE_THRESHOLDIMAGECHANNEL)
                         ThresholdImageChannel
#else
                         ChannelThresholdImage
#endif
                                              );
}


/*
    Method:     Image#charcoal(radius=0.0, sigma=1.0)
    Purpose:    Return a new image that is a copy of the input image with the
                edges highlighted
*/
VALUE
Image_charcoal(int argc, VALUE *argv, VALUE self)
{
    return effect_image(self, argc, argv, CharcoalImage);
}

/*
    Method:     Image#chop
    Purpose:    removes a region of an image and collapses the image to occupy
                the removed portion
*/
VALUE
Image_chop(
    VALUE self,
    VALUE x,
    VALUE y,
    VALUE width,
    VALUE height)
{
    return xform_image(False, self, x, y, width, height, ChopImage);
}

/*
    Method:     Image#chromaticity
    Purpose:    Return the red, green, blue, and white-point chromaticity
                values as a Magick::ChromaticityInfo.
*/
VALUE
Image_chromaticity(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return ChromaticityInfo_new(&image->chromaticity);
}

/*
    Method:     Image#chromaticity=
    Purpose:    Set the red, green, blue, and white-point chromaticity
                values from a Magick::ChromaticityInfo.
*/
VALUE
Image_chromaticity_eq(VALUE self, VALUE chroma)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    ChromaticityInfo_to_ChromaticityInfo(&image->chromaticity, chroma);
    return self;
}


/*
    Method:     Image#clone
    Purpose:    Copy an image, along with its frozen and tainted state.
*/
VALUE
Image_clone(VALUE self)
{
    volatile VALUE clone;

    clone = Image_dup(self);
    if (OBJ_FROZEN(self))
    {
        (void) rb_obj_freeze(clone);
    }

    return clone;
}

/*
    Method:     Image_color_histogram(VALUE self);
    Purpose:    Call GetColorHistogram (>= GM 1.1)
                     GetImageHistogram (>= IM 5.5.8)
    Notes:      returns hash {aPixel=>count}
*/
VALUE
Image_color_histogram(VALUE self)
{
#if defined(HAVE_GETCOLORHISTOGRAM)
    Image *image;
    volatile VALUE hash, pixel;
    unsigned long x, colors;
    HistogramColorPacket *histogram;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    GetExceptionInfo(&exception);
    histogram = GetColorHistogram(image, &colors, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    hash = rb_hash_new();
    for (x = 0; x < colors; x++)
    {
        pixel = Pixel_from_PixelPacket(&histogram[x].pixel);
        rb_hash_aset(hash, pixel, ULONG2NUM(histogram[x].count));
    }

    /*
        The histogram array is specifically allocated by malloc because it is
        supposed to be freed by the caller.
    */
    free(histogram);

    return hash;


#elif defined(HAVE_GETIMAGEHISTOGRAM)
    Image *image, *dc_copy = NULL;
    volatile VALUE hash, pixel;
    unsigned long x, colors;
    ColorPacket *histogram;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    // If image not DirectClass make a DirectClass copy.
    if (image->storage_class != DirectClass)
    {
        dc_copy = rm_clone_image(image);
        (void) SyncImage(dc_copy);
        magick_free(dc_copy->colormap);
        dc_copy->colormap = NULL;
        dc_copy->storage_class = DirectClass;
        image = dc_copy;
    }

    GetExceptionInfo(&exception);
    histogram = GetImageHistogram(image, &colors, &exception);

    if (histogram == NULL)
    {
        if (dc_copy)
        {
            (void) DestroyImage(dc_copy);
        }
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }
    if (exception.severity != UndefinedException)
    {
        (void) RelinquishMagickMemory(histogram);
        rm_check_exception(&exception, dc_copy, DestroyOnError);
    }

    (void) DestroyExceptionInfo(&exception);

    hash = rb_hash_new();
    for (x = 0; x < colors; x++)
    {
        pixel = Pixel_from_PixelPacket(&histogram[x].pixel);
        (void) rb_hash_aset(hash, pixel, ULONG2NUM((unsigned long)histogram[x].count));
    }

    /*
        Christy evidently didn't agree with Bob's memory management.
    */
    (void) RelinquishMagickMemory(histogram);

    if (dc_copy)
    {
        (void) DestroyImage(dc_copy);
    }

    return hash;
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}



/*
    Static:     set_profile(target_image, name, profile_image)
    Purpose:    The `profile_image' argument is an IPTC or ICC profile. Store
                all the profiles in the profile in the target image.
                Called from Image_color_profile_eq and Image_iptc_profile_eq
*/
static VALUE set_profile(VALUE self, const char *name, VALUE profile)
{
#if defined(HAVE_GETNEXTIMAGEPROFILE)
    Image *image, *profile_image;
    ImageInfo *info;
    const MagickInfo *m;
    ExceptionInfo exception;
    char *profile_name;
    char *profile_blob;
    long profile_length;
    const StringInfo *profile_data;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    profile_blob = STRING_PTR_LEN(profile, profile_length);

    GetExceptionInfo(&exception);
    m = GetMagickInfo(name, &exception);
    CHECK_EXCEPTION()

    info = CloneImageInfo(NULL);
    if (!info)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }

    strncpy(info->magick, m->name, MaxTextExtent);
    info->magick[MaxTextExtent-1] = '\0';

    profile_image = BlobToImage(info, profile_blob, (size_t)profile_length, &exception);
    (void) DestroyImageInfo(info);
    CHECK_EXCEPTION()
    (void) DestroyExceptionInfo(&exception);

    ResetImageProfileIterator(profile_image);
    profile_name = GetNextImageProfile(profile_image);
    while (profile_name)
    {
        if (rm_strcasecmp(profile_name, name) == 0)
        {
            profile_data = GetImageProfile(profile_image, profile_name);
            if (profile)
            {
                (void)ProfileImage(image, profile_name, profile_data->datum
                                 , (unsigned long)profile_data->length
                                 , (MagickBooleanType)False);
                if (image->exception.severity >= ErrorException)
                {
                    break;
                }
            }
        }
        profile_name = GetNextImageProfile(profile_image);
    }

    (void) DestroyImage(profile_image);
    rm_check_image_exception(image, RetainOnError);

#else

    Image *image, *profile_image;
    ImageInfo *info;
    ExceptionInfo exception;
    const MagickInfo *m;
    char *profile_blob;
    long profile_length;
    const unsigned char *profile_data;
    size_t profile_data_l;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    profile_blob = STRING_PTR_LEN(profile, profile_length);

    GetExceptionInfo(&exception);
    m = GetMagickInfo(name, &exception);
    CHECK_EXCEPTION()

    info = CloneImageInfo(NULL);
    if (!info)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }

    strncpy(info->magick, m->name, MaxTextExtent);
    info->magick[MaxTextExtent-1] = '\0';

    profile_image = BlobToImage(info, profile_blob, profile_length, &exception);
    (void) DestroyImageInfo(info);
    CHECK_EXCEPTION()
    (void) DestroyExceptionInfo(&exception);

    // GraphicsMagick uses "ICM" to refer to the ICC profile.
    if (rm_strcasecmp(name, "ICC") == 0)
    {
        profile_data = GetImageProfile(profile_image, "ICM", &profile_data_l);
    }
    else
    {
        profile_data = GetImageProfile(profile_image, name, &profile_data_l);
    }
    if (profile_data)
    {
        (void)SetImageProfile(image, name, profile_data, profile_data_l);
    }

    (void) DestroyImage(profile_image);
    rm_check_image_exception(image, RetainOnError);

#endif

    return self;
}


/*
    Method:     Image#color_profile
    Purpose:    Return the ICC color profile as a String.
    Notes:      If there is no profile, returns ""
                This method has no real use but is retained for compatibility
                with earlier releases of RMagick, where it had no real use either.
*/
VALUE
Image_color_profile(VALUE self)
{
    Image *image;

#if defined(HAVE_ACQUIRESTRINGINFO)

    const StringInfo *profile;

    Data_Get_Struct(self, Image, image);
    profile = GetImageProfile(image, "icc");
    if (!profile)
    {
        return Qnil;
    }

    return rb_str_new((char *)profile->datum, (long)profile->length);

#else

    const unsigned char *profile;
    size_t length;

    Data_Get_Struct(self, Image, image);

    profile = GetImageProfile(image, "ICM", &length);
    if (!profile)
    {
        return Qnil;
    }

    return rb_str_new((char *)profile, (long)length);

#endif
}

/*
    Method:     Image#color_profile=(String)
    Purpose:    Set the ICC color profile. The argument is a string.
    Notes:      Pass nil to remove any existing profile.
                Removes any existing profile before adding the new one.
*/
VALUE
Image_color_profile_eq(VALUE self, VALUE profile)
{
    (void) Image_delete_profile(self, rb_str_new2("ICC"));
    if (profile != Qnil)
    {
        (void) set_profile(self, "ICC", profile);
    }
    return self;
}

/*
    Method:     Image#color_flood_fill(target_color, fill_color, x, y, method)
    Purpose:    changes the color value of any pixel that matches target_color
                and is an immediate neighbor.
    Notes:      use fuzz= to specify the tolerance amount (see Image_opaque)
                Accepts either the FloodfillMethod or the FillToBorderMethod
*/
VALUE
Image_color_flood_fill(
    VALUE self,
    VALUE target_color,
    VALUE fill_color,
    VALUE xv,
    VALUE yv,
    VALUE method)
{
    Image *image, *new_image;
    PixelPacket target;
    DrawInfo *draw_info;
    PixelPacket fill;
    long x, y;
    int fill_method;

    Data_Get_Struct(self, Image, image);

    // The target and fill args can be either a color name or
    // a Magick::Pixel.
    Color_to_PixelPacket(&target, target_color);
    Color_to_PixelPacket(&fill, fill_color);

    x = NUM2LONG(xv);
    y = NUM2LONG(yv);
    if ((unsigned long)x > image->columns || (unsigned long)y > image->rows)
    {
        rb_raise(rb_eArgError, "target out of range. %lux%lu given, image is %lux%lu"
               , x, y, image->columns, image->rows);
    }

    VALUE_TO_ENUM(method, fill_method, PaintMethod);
    if (!(fill_method == FloodfillMethod || fill_method == FillToBorderMethod))
    {
        rb_raise(rb_eArgError, "paint method must be FloodfillMethod or "
                               "FillToBorderMethod (%d given)", fill_method);
    }

    draw_info = CloneDrawInfo(NULL, NULL);
    if (!draw_info)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }
    draw_info->fill = fill;

    new_image = rm_clone_image(image);

    (void) ColorFloodfillImage(new_image, draw_info, target, x, y, (PaintMethod)fill_method);
    // No need to check for error

    (void) DestroyDrawInfo(draw_info);
    return rm_image_new(new_image);
}

/*
    Method:     Image#colorize(r, g, b, target)
    Purpose:    blends the fill color specified by "target" with each pixel in
                the image. Specify the percentage blend for each r, g, b
                component.
*/
VALUE
Image_colorize(
    int argc,
    VALUE *argv,
    VALUE self)
{
    Image *image, *new_image;
    double red, green, blue, matte;
    char opacity[50];
    PixelPacket target;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    if (argc == 4)
    {
        red   = floor(100*NUM2DBL(argv[0])+0.5);
        green = floor(100*NUM2DBL(argv[1])+0.5);
        blue  = floor(100*NUM2DBL(argv[2])+0.5);
        Color_to_PixelPacket(&target, argv[3]);
        sprintf(opacity, "%f/%f/%f", red, green, blue);
    }
    else if (argc == 5)
    {
        red   = floor(100*NUM2DBL(argv[0])+0.5);
        green = floor(100*NUM2DBL(argv[1])+0.5);
        blue  = floor(100*NUM2DBL(argv[2])+0.5);
        matte = floor(100*NUM2DBL(argv[3])+0.5);
        Color_to_PixelPacket(&target, argv[4]);
        sprintf(opacity, "%f/%f/%f/%f", red, green, blue, matte);
    }
    else
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 4 or 5)", argc);
    }

    GetExceptionInfo(&exception);
    new_image = ColorizeImage(image, opacity, target, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}


/*
    Method:     Image#colormap(index<, new-color>)
    Purpose:    return the color in the colormap at the specified index. If
                a new color is specified, replaces the color at the index
                with the new color.
    Returns:    the name of the color.
    Notes:      The "new-color" argument can be either a color name or
                a Magick::Pixel.
*/
VALUE
Image_colormap(int argc, VALUE *argv, VALUE self)
{
    Image *image;
    unsigned long index;
    PixelPacket color, new_color;

    Data_Get_Struct(self, Image, image);

    // We can handle either 1 or 2 arguments. Nothing else.
    if (argc == 0 || argc > 2)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 or 2)", argc);
    }

    index = NUM2ULONG(argv[0]);
    if (index > MaxRGB)
    {
        rb_raise(rb_eIndexError, "index out of range");
    }

    // If this is a simple "get" operation, ensure the image has a colormap.
    if (argc == 1)
    {
        if (!image->colormap)
        {
            rb_raise(rb_eIndexError, "image does not contain a colormap");
        }
        // Validate the index

        if (index > image->colors-1)
        {
            rb_raise(rb_eIndexError, "index out of range");
        }
        return PixelPacket_to_Color_Name(image, &image->colormap[index]);
    }

    // This is a "set" operation. Things are different.

    rm_check_frozen(self);

    // Replace with new color? The arg can be either a color name or
    // a Magick::Pixel.
    Color_to_PixelPacket(&new_color, argv[1]);

    // Handle no colormap or current colormap too small.
    if (!image->colormap || index > image->colors-1)
    {
        PixelPacket black = {0};
        unsigned long i;

        if (!image->colormap)
        {
            image->colormap = (PixelPacket *)magick_malloc((index+1)*sizeof(PixelPacket));
            image->colors = 0;
        }
        else
        {
            image->colormap = magick_realloc(image->colormap, (index+1)*sizeof(PixelPacket));
        }

        for (i = image->colors; i < index; i++)
        {
            image->colormap[i] = black;
        }
        image->colors = index+1;
    }

    // Save the current color so we can return it. Set the new color.
    color = image->colormap[index];
    image->colormap[index] = new_color;

    return PixelPacket_to_Color_Name(image, &color);
}

DEF_ATTR_READER(Image, colors, ulong)

/*
    Method:     Image#colorspace
    Purpose:    Return theImage pixel interpretation. If the colorspace is
                RGB the pixels are red, green, blue. If matte is true, then
                red, green, blue, and index. If it is CMYK, the pixels are
                cyan, yellow, magenta, black. Otherwise the colorspace is
                ignored.
*/
VALUE
Image_colorspace(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return ColorspaceType_new(image->colorspace);
}

/*
    Method:     Image#colorspace=Magick::ColorspaceType
    Purpose:    Set the image's colorspace
    Notes:      Ref: Magick++'s Magick::colorSpace method
*/
VALUE
Image_colorspace_eq(VALUE self, VALUE colorspace)
{
    Image *image;
    ColorspaceType new_cs;

    rm_check_frozen(self);
    VALUE_TO_ENUM(colorspace, new_cs, ColorspaceType);
    Data_Get_Struct(self, Image, image);

#if defined(HAVE_SETIMAGECOLORSPACE)

    // SetImageColorspace was introduced in 5.5.7. It is essentially
    // identical to the code below. It either works or throws an exception.
    (void) SetImageColorspace(image, new_cs);
    // No need to check for errors

#else

    if (new_cs == image->colorspace)
    {
        return self;
    }

    if (new_cs != RGBColorspace &&
        new_cs != TransparentColorspace &&
        new_cs != GRAYColorspace)
    {
        if (image->colorspace != RGBColorspace &&
            image->colorspace != TransparentColorspace &&
            image->colorspace != GRAYColorspace)
        {
           TransformRGBImage(image, image->colorspace);
        }
        RGBTransformImage(image, new_cs);
    }
    else if (new_cs == RGBColorspace || new_cs == TransparentColorspace || new_cs == GRAYColorspace)
    {
        TransformRGBImage(image, image->colorspace);
    }
#endif

    return self;
}


DEF_ATTR_READER(Image, columns, int)


/*
    Method:     Image#compare_channel(ref_image, metric [, channel...])
    Purpose:    compares one or more channels in two images and returns
                the specified distortion metric and a comparison image.
    Notes:      If no channels are specified, the default is AllChannels.
                That case is the equivalent of the CompareImages method in
                ImageMagick.

                Originally this method was called channel_compare, but
                that doesn't match the general naming convention that
                methods which accept multiple optional ChannelType
                arguments have names that end in _channel.  So I renamed
                the method to compare_channel but kept channel_compare as
                an alias.
*/
VALUE Image_compare_channel(
    int argc,
    VALUE *argv,
    VALUE self)
{
#if defined(HAVE_COMPAREIMAGECHANNELS)

    Image *image, *r_image, *difference_image;
    double distortion;
    volatile VALUE ary;
    MetricType metric_type;
    ChannelType channels;
    ExceptionInfo exception;

    channels = extract_channels(&argc, argv);
    if (argc > 2)
    {
        raise_ChannelType_error(argv[argc-1]);
    }
    if (argc != 2)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 or more)", argc);
    }

    Data_Get_Struct(self, Image, image);
    Data_Get_Struct(ImageList_cur_image(argv[0]), Image, r_image);
    VALUE_TO_ENUM(argv[1], metric_type, MetricType);

    GetExceptionInfo(&exception);
    difference_image = CompareImageChannels(image
                                            , r_image
                                            , channels
                                            , metric_type
                                            , &distortion
                                            , &exception);
    rm_check_exception(&exception, difference_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(difference_image);

    ary = rb_ary_new2(2);
    rb_ary_store(ary, 0, rm_image_new(difference_image));
    rb_ary_store(ary, 1, rb_float_new(distortion));

    return ary;
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#compose -> composite_op
    Purpose:    Return the composite operator attribute
*/
VALUE Image_compose(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);

    return CompositeOperator_new(image->compose);
}


/*
    Method:     Image#compose=composite_op
    Purpose:    Set the composite operator attribute
*/
VALUE Image_compose_eq(
    VALUE self,
    VALUE compose_arg)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    VALUE_TO_ENUM(compose_arg, image->compose, CompositeOperator);
    return self;
}

/*
    Method:     Image#composite(image, x_off, y_off, composite_op)
                Image#composite(image, gravity, composite_op)
                Image#composite(image, gravity, x_off, y_off, composite_op)
    Purpose:    Call CompositeImage
    Notes:      the other image can be either an Image or an Image.
                The use of the GravityType to position the composited
                image is based on Magick++. The `gravity' argument has
                the same effect as the -gravity option does in the
                `composite' utility.
    Returns:    new composited image, or nil
*/

static VALUE composite(
    int bang,
    int argc,
    VALUE *argv,
    VALUE self,
    ChannelType channels)
{
    Image *image, *new_image;
    Image *comp_image;
    CompositeOperator operator;
    GravityType gravity;
    MagickEnum *magick_enum;
    signed long x_offset;
    signed long y_offset;

    Data_Get_Struct(self, Image, image);

    switch (argc)
    {
        case 3:                 // argv[1] is gravity, argv[2] is composite_op
            Data_Get_Struct(ImageList_cur_image(argv[0]), Image, comp_image);
            VALUE_TO_ENUM(argv[1], gravity, GravityType);
            VALUE_TO_ENUM(argv[2], operator, CompositeOperator);

                                // convert gravity to x, y offsets
            switch (gravity)
            {
                case ForgetGravity:
                case NorthWestGravity:
                    x_offset = 0;
                    y_offset = 0;
                break;
                case NorthGravity:
                    x_offset = ((long)(image->columns) - (long)(comp_image->columns)) / 2;
                    y_offset = 0;
                break;
                case NorthEastGravity:
                    x_offset = (long)(image->columns) - (long)(comp_image->columns);
                    y_offset = 0;
                break;
                case WestGravity:
                    x_offset = 0;
                    y_offset = ((long)(image->rows) - (long)(comp_image->rows)) / 2;
                break;
                case StaticGravity:
                case CenterGravity:
                default:
                    x_offset = ((long)(image->columns) - (long)(comp_image->columns)) / 2;
                    y_offset = ((long)(image->rows) - (long)(comp_image->rows)) / 2;
                break;
                case EastGravity:
                    x_offset = (long)(image->columns) - (long)(comp_image->columns);
                    y_offset = ((long)(image->rows) - (long)(comp_image->rows)) / 2;
                break;
                case SouthWestGravity:
                    x_offset = 0;
                    y_offset = (long)(image->rows) - (long)(comp_image->rows);
                break;
                case SouthGravity:
                    x_offset = ((long)(image->columns) - (long)(comp_image->columns)) / 2;
                    y_offset = (long)(image->rows) - (long)(comp_image->rows);
                break;
                case SouthEastGravity:
                    x_offset = (long)(image->columns) - (long)(comp_image->columns);
                    y_offset = (long)(image->rows) - (long)(comp_image->rows);
                break;
            }
            break;

        case 4:                 // argv[1], argv[2] is x_off, y_off,
                                // argv[3] is composite_op
            Data_Get_Struct(ImageList_cur_image(argv[0]), Image, comp_image);
            x_offset = NUM2LONG(argv[1]);
            y_offset = NUM2LONG(argv[2]);
            VALUE_TO_ENUM(argv[3], operator, CompositeOperator);
            break;

        case 5:
            Data_Get_Struct(ImageList_cur_image(argv[0]), Image, comp_image);
            VALUE_TO_ENUM(argv[1], gravity, GravityType);
            x_offset = NUM2LONG(argv[2]);
            y_offset = NUM2LONG(argv[3]);
            VALUE_TO_ENUM(argv[4], operator, CompositeOperator);

            switch(gravity)
            {
                case NorthEastGravity:
                case EastGravity:
                case SouthEastGravity:
                    x_offset = ((long)(image->columns) - (long)(comp_image->columns)) - x_offset;
                    break;
                case NorthGravity:
                case SouthGravity:
                case CenterGravity:
                case StaticGravity:
                    x_offset += (long)(image->columns/2) - (long)(comp_image->columns/2);
                    break;
                default:
                    break;
            }
            switch(gravity)
            {
                case SouthWestGravity:
                case SouthGravity:
                case SouthEastGravity:
                    y_offset = ((long)(image->rows) - (long)(comp_image->rows)) - y_offset;
                    break;
                case EastGravity:
                case WestGravity:
                case CenterGravity:
                case StaticGravity:
                    y_offset += (long)(image->rows/2) - (long)(comp_image->rows/2);
                    break;
                case NorthEastGravity:
                case NorthGravity:
                    // Don't let these run into the default case
                    break;
                default:
                    Data_Get_Struct(argv[1], MagickEnum, magick_enum);
                    rb_warning("gravity type `%s' has no effect", rb_id2name(magick_enum->id));
                    break;
            }
            break;

        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 3, 4, or 5)", argc);
            break;
    }

    if (bang)
    {
        rm_check_frozen(self);
#if defined(HAVE_COMPOSITEIMAGECHANNEL)
        (void) CompositeImageChannel(image, channels, operator, comp_image, x_offset, y_offset);
#else
        (void) CompositeImage(image, operator, comp_image, x_offset, y_offset);
#endif
        rm_check_image_exception(image, RetainOnError);
        return self;
    }
    else
    {
        new_image = rm_clone_image(image);

#if defined(HAVE_COMPOSITEIMAGECHANNEL)
        (void) CompositeImageChannel(new_image, channels, operator, comp_image, x_offset, y_offset);
#else
        (void) CompositeImage(new_image, operator, comp_image, x_offset, y_offset);
#endif
        rm_check_image_exception(new_image, DestroyOnError);

        return rm_image_new(new_image);
    }
}


VALUE Image_composite_bang(
    int argc,
    VALUE *argv,
    VALUE self)
{
#if defined(HAVE_ALLCHANNELS)
        ChannelType channels = (AllChannels &~ OpacityChannel);
#else
        ChannelType channels = (0xff &~ OpacityChannel);
#endif
    return composite(True, argc, argv, self, channels);
}

VALUE Image_composite(
    int argc,
    VALUE *argv,
    VALUE self)
{
#if defined(HAVE_ALLCHANNELS)
        ChannelType channels = (AllChannels &~ OpacityChannel);
#else
        ChannelType channels = (0xff &~ OpacityChannel);
#endif
    return composite(False, argc, argv, self, channels);
}


/*
    Method:     Image#composite_affine(composite, affine_matrix)
    Purpose:    composites the source over the destination image as
                dictated by the affine transform.
*/
VALUE
Image_composite_affine(
    VALUE self,
    VALUE source,
    VALUE affine_matrix)
{
    Image *image, *composite, *new_image;
    AffineMatrix affine;

    Data_Get_Struct(self, Image, image);
    Data_Get_Struct(source, Image, composite);

    new_image = rm_clone_image(image);

    AffineMatrix_to_AffineMatrix(&affine, affine_matrix);
    (void) DrawAffineImage(new_image, composite, &affine);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}


/*
    Method:     Image#composite_channel(src_image, geometry, composite_operator[, channel...])
                Image#composite_channel!(src_image, geometry, composite_operator[, channel...])
    Purpose:    Call CompositeImageChannel
*/
static VALUE
composite_channel(int bang, int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_COMPOSITEIMAGECHANNEL)
    ChannelType channels;

    channels = extract_channels(&argc, argv);

    // There must be 3, 4, or 5 remaining arguments.
    if (argc < 3)
    {
        rb_raise(rb_eArgError, "composite operator not specified");
    }
    else if (argc > 5)
    {
        raise_ChannelType_error(argv[argc-1]);
    }

    return composite(bang, argc, argv, self, channels);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

VALUE Image_composite_channel(int argc, VALUE *argv, VALUE self)
{
    return composite_channel(False, argc, argv, self);
}


VALUE Image_composite_channel_bang(int argc, VALUE *argv, VALUE self)
{
    return composite_channel(True, argc, argv, self);
}


/*
    Method:     Image#compression
                Image#compression=
    Purpose:    Get/set the compresion attribute
*/
VALUE
Image_compression(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return CompressionType_new(image->compression);
}

VALUE
Image_compression_eq(VALUE self, VALUE compression)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    VALUE_TO_ENUM(compression, image->compression, CompressionType);
    return self;
}

/*
    Method:     Image#compress_colormap!
    Purpose:    call CompressImageColormap
    Notes:      API was CompressColormap until 5.4.9
*/
VALUE
Image_compress_colormap_bang(VALUE self)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    (void) CompressImageColormap(image);
    rm_check_image_exception(image, RetainOnError);

    return self;
}

/*
    Method:     Image.constitute(width, height, map, pixels)
    Purpose:    Creates an Image from the supplied pixel data. The
                pixel data must be in scanline order, top-to-bottom.
                The pixel data is an array of either all Fixed or all Float
                elements. If Fixed, the elements must be in the range
                [0..MaxRGB]. If Float, the elements must be normalized [0..1].
                The "map" argument reflects the expected ordering of the pixel
                array. It can be any combination or order of R = red, G = green,
                B = blue, A = alpha, C = cyan, Y = yellow, M = magenta,
                K = black, or I = intensity (for grayscale).

                The pixel array must have width X height X strlen(map) elements.
    Raises:     ArgumentError, TypeError
*/

VALUE
Image_constitute(VALUE class, VALUE width_arg, VALUE height_arg
                    , VALUE map_arg, VALUE pixels_arg)
{
    Image *image;
    ExceptionInfo exception;
    volatile VALUE pixel, pixel0;
    unsigned long width, height;
    long x, npixels;
    char *map;
    long map_l;
    union
    {
       float *f;
       Quantum *i;
       void *v;
    } pixels;
    int type;
    StorageType stg_type;

    class = class;  // Suppress "never referenced" message from icc

    // rb_Array converts objects that are not Arrays to Arrays if possible,
    // and raises TypeError if it can't.
    pixels_arg = rb_Array(pixels_arg);

    width = NUM2ULONG(width_arg);
    height = NUM2ULONG(height_arg);

    if (width == 0 || height == 0)
    {
        rb_raise(rb_eArgError, "width and height must be non-zero");
    }

    map = STRING_PTR_LEN(map_arg, map_l);

    npixels = (long)(width * height * map_l);
    if (RARRAY(pixels_arg)->len != npixels)
    {
        rb_raise(rb_eArgError, "wrong number of array elements (%ld for %ld)"
               , RARRAY(pixels_arg)->len, npixels);
    }

    // Inspect the first element in the pixels array to determine the expected
    // type of all the elements. Allocate the pixel buffer.
    pixel0 = rb_ary_entry(pixels_arg, 0);
    if (TYPE(pixel0) == T_FLOAT)
    {
        pixels.f = ALLOC_N(float, npixels);
        stg_type = FloatPixel;
    }
    else if (TYPE(pixel0) == T_FIXNUM)
    {
        pixels.i = ALLOC_N(Quantum, npixels);
        stg_type = FIX_STG_TYPE;
    }
    else
    {
        rb_raise(rb_eTypeError, "element 0 in pixel array is %s, must be Fixnum or Double"
               , rb_class2name(CLASS_OF(pixel0)));
    }

    type = TYPE(pixel0);

    // Convert the array elements to the appropriate C type, store in pixel
    // buffer.
    for (x = 0; x < npixels; x++)
    {
        pixel = rb_ary_entry(pixels_arg, x);
        if (TYPE(pixel) != type)
        {
            rb_raise(rb_eTypeError, "element %ld in pixel array is %s, expected %s"
                   , x, rb_class2name(CLASS_OF(pixel)),rb_class2name(CLASS_OF(pixel0)));
        }
        if (type == T_FLOAT)
        {
            pixels.f[x] = (float) NUM2DBL(pixel);
            if (pixels.f[x] < 0.0 || pixels.f[x] > 1.0)
            {
                rb_raise(rb_eArgError, "element %ld is out of range [0..1]: %f", x, pixels.f[x]);
            }
        }
        else
        {
            pixels.i[x] = (Quantum)FIX2LONG(pixel);
        }
    }

    // Release the pixel buffer before any exception can be raised.
    GetExceptionInfo(&exception);

#if defined(HAVE_IMPORTIMAGEPIXELS)

    // This is based on ConstituteImage in IM 5.5.7
    image = AllocateImage(NULL);
    if (!image)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue.");
    }
    SetImageExtent(image, width, height);
    (void) SetImageBackgroundColor(image);

    (void) ImportImagePixels(image, 0, 0, width, height, map, stg_type, pixels.v);
    rm_check_image_exception(image, DestroyOnError);
#else
    image = ConstituteImage(width, height, map, stg_type, pixels.v, &exception);
    rm_check_exception(&exception, image, DestroyOnError);
#endif

    (void) DestroyExceptionInfo(&exception);
    DestroyConstitute();

    xfree(pixels.v);

    return rm_image_new(image);
}

/*
    Method:     Image#contrast(<sharpen>)
    Purpose:    enhances the intensity differences between the lighter and
                darker elements of the image. Set sharpen to "true" to
                increase the image contrast otherwise the contrast is reduced.
    Notes:      if omitted, "sharpen" defaults to 0
    Returns:    new contrasted image
*/
VALUE
Image_contrast(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    unsigned int sharpen = 0;

    if (argc > 1)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 or 1)", argc);
    }
    else if (argc == 1)
    {
        sharpen = RTEST(argv[0]);
    }

    Data_Get_Struct(self, Image, image);
    new_image = rm_clone_image(image);

    (void) ContrastImage(new_image, sharpen);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}


/*
    Static:     get_black_white_point
    Purpose:    Convert percentages to #pixels. If the white-point (2nd)
                argument is not supplied set it to #pixels - black-point.
*/
static void get_black_white_point(
    Image *image,
    int argc,
    VALUE *argv,
    double *black_point,
    double *white_point)
{
    double pixels;

    pixels = (double) (image->columns * image->rows);

    switch (argc)
    {
        case 2:
            if (rm_check_num2dbl(argv[0]))
            {
                *black_point = NUM2DBL(argv[0]);
            }
            else
            {
                *black_point = pixels * rm_str_to_pct(argv[0]);
            }
            if (rm_check_num2dbl(argv[1]))
            {
                *white_point = NUM2DBL(argv[1]);
            }
            else
            {
                *white_point = pixels * rm_str_to_pct(argv[1]);
            }
            break;

        case 1:
            if (rm_check_num2dbl(argv[0]))
            {
                *black_point = NUM2DBL(argv[0]);
            }
            else
            {
                *black_point = pixels * rm_str_to_pct(argv[0]);
            }
            *white_point = pixels - *black_point;
            break;

        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 or 2)", argc);
            break;
    }

    return;
}


/*
    Method:     Image#contrast_stretch_channel(black_point <, white_point> <, channel...>)
    Purpose:    Call ContrastStretchImageChannel
    Notes:      If white_point is not specified then it is #pixels-black_point.
                Both black_point and white_point can be specified as Floats
                or as percentages, i.e. "10%"
*/
VALUE
Image_contrast_stretch_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_CONTRASTSTRETCHIMAGECHANNEL)
    Image *image, *new_image;
    ChannelType channels;
    double black_point, white_point;

    channels = extract_channels(&argc, argv);
    if (argc > 2)
    {
        raise_ChannelType_error(argv[argc-1]);
    }

    Data_Get_Struct(self, Image, image);

    get_black_white_point(image, argc, argv, &black_point, &white_point);

    new_image = rm_clone_image(image);

    (void) ContrastStretchImageChannel(new_image, channels, black_point, white_point);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:     Image#convolve(order, kernel)
    Purpose:    apply a custom convolution kernel to the image
    Notes:      "order" is the number of rows and columns in the kernel
                "kernel" is an order**2 array of doubles
*/
VALUE
Image_convolve(
    VALUE self,
    VALUE order_arg,
    VALUE kernel_arg)
{
    Image *image, *new_image;
    volatile double *kernel;
    unsigned int x, order;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    order = NUM2UINT(order_arg);

    kernel_arg = rb_Array(kernel_arg);
    rm_check_ary_len(kernel_arg, (long)(order*order));

    // Convert the kernel array argument to an array of doubles

    kernel = (volatile double *)ALLOC_N(double, order*order);
    for (x = 0; x < order*order; x++)
    {
        kernel[x] = NUM2DBL(rb_ary_entry(kernel_arg, (long)x));
    }

    GetExceptionInfo(&exception);

    new_image = ConvolveImage(image, order, (double *)kernel, &exception);
    xfree((void *)kernel);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}


/*
 *  Method:     Image#convolve_channel(order, kernel[, channel[, channel...]])
 *  Purpose:    call ConvolveImageChannel
*/
VALUE
Image_convolve_channel(
    int argc,
    VALUE *argv,
    VALUE self)
{
#if defined(HAVE_CONVOLVEIMAGECHANNEL)
    Image *image, *new_image;
    volatile double *kernel;
    volatile VALUE ary;
    unsigned int x, order;
    ChannelType channels;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    channels = extract_channels(&argc, argv);

    // There are 2 required arguments.
    if (argc > 2)
    {
        raise_ChannelType_error(argv[argc-1]);
    }
    if (argc != 2)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 or more)", argc);
    }

    order = NUM2UINT(argv[0]);
    ary = argv[1];

    rm_check_ary_len(ary, (long)(order*order));

    kernel = ALLOC_N(double, (long)(order*order));

    // Convert the kernel array argument to an array of doubles
    for (x = 0; x < order*order; x++)
    {
        kernel[x] = NUM2DBL(rb_ary_entry(ary, (long)x));
    }

    GetExceptionInfo(&exception);

    new_image = ConvolveImageChannel(image, channels, order, (double *)kernel, &exception);
    xfree((void *)kernel);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}



/*
    Method:     Image#copy
    Purpose:    Alias for dup
*/
VALUE
Image_copy(VALUE self)
{
    return rb_funcall(self, rm_ID_dup, 0);
}

/*
    Method:     Image#initialize_copy
    Purpose:    initialize copy, clone, dup
*/
VALUE
Image_init_copy(VALUE copy, VALUE orig)
{
    Image *image;

    Data_Get_Struct(orig, Image, image);
    DATA_PTR(copy) = rm_clone_image(image);

    return copy;
}


/*
    Method:     Image#crop(x, y, width, height)
                Image#crop(gravity, width, height)
                Image#crop!(x, y, width, height)
                Image#crop!(gravity, width, height)
    Purpose:    Extract a region of the image defined by width, height, x, y
*/
VALUE
Image_crop(int argc, VALUE *argv, VALUE self)
{
    return cropper(False, argc, argv, self);
}

VALUE
Image_crop_bang(int argc, VALUE *argv, VALUE self)
{
    rm_check_frozen(self);
    return cropper(True, argc, argv, self);
}

/*
    Method:     Image#cycle_colormap
    Purpose:    Call CycleColormapImage
*/
VALUE
Image_cycle_colormap(VALUE self, VALUE amount)
{
    Image *image, *new_image;
    int amt;

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);
    amt = NUM2INT(amount);
    (void) CycleColormapImage(new_image, amt);
    // No need to check for an error

    return rm_image_new(new_image);
}

/*
    Method:     Image#density
    Purpose:    Get the x & y resolutions.
    Returns:    A string in the form "XresxYres"
*/
VALUE
Image_density(VALUE self)
{
    Image *image;
    char density[128];

    Data_Get_Struct(self, Image, image);

    sprintf(density, "%gx%g", image->x_resolution, image->y_resolution);
    return rb_str_new2(density);
}

/*
    Method:     Image#density="XxY"
                Image#density=aGeometry
    Purpose:    Set the x & y resolutions in the image
    Notes:      The density is a string of the form "XresxYres" or simply "Xres".
                If the y resolution is not specified, set it equal to the x
                resolution. This is equivalent to PerlMagick's handling of
                density.

                The density can also be a Geometry object. The width attribute
                is used for the x resolution. The height attribute is used for
                the y resolution. If the height attribute is missing, the
                width attribute is used for both.
*/

VALUE
Image_density_eq(VALUE self, VALUE density_arg)
{
    Image *image;
    char *density;
    volatile VALUE x_val, y_val;
    int count;
    double x_res, y_res;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    // Get the Class ID for the Geometry class.
    if (!Class_Geometry)
    {
        Class_Geometry = rb_const_get(Module_Magick, rm_ID_Geometry);
    }

    // Geometry object. Width and height attributes are always positive.
    if (CLASS_OF(density_arg) == Class_Geometry)
    {
        x_val = rb_funcall(density_arg, rm_ID_width, 0);
        x_res = NUM2DBL(x_val);
        y_val = rb_funcall(density_arg, rm_ID_height, 0);
        y_res = NUM2DBL(y_val);
        if(x_res == 0.0)
        {
            rb_raise(rb_eArgError, "invalid x resolution: %f", x_res);
        }
        image->y_resolution = y_res != 0.0 ? y_res : x_res;
        image->x_resolution = x_res;
    }

    // Convert the argument to a string
    else
    {
        density = STRING_PTR(density_arg);
        if (!IsGeometry(density))
        {
            rb_raise(rb_eArgError, "invalid density geometry %s", density);
        }

        count = sscanf(density, "%lfx%lf", &image->x_resolution, &image->y_resolution);
        if (count < 2)
        {
            image->y_resolution = image->x_resolution;
        }

    }

    return self;
}

/*
    Method:     Image#depth
    Purpose:    Return the image depth (8 or 16).
    Note:       If all pixels have lower-order bytes equal to higher-order
                bytes, the depth will be reported as 8 even if the depth
                field in the Image structure says 16.
*/
VALUE
Image_depth(VALUE self)
{
    Image *image;
    unsigned long depth = 0;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    depth = GetImageDepth(image, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return INT2FIX(depth);
}


DEF_ATTR_ACCESSOR(Image, delay, ulong)


/*
    Method:     Image#delete_profile(name)
    Purpose:    call ProfileImage
    Notes:      name is the name of the profile to be deleted
*/
VALUE
Image_delete_profile(VALUE self, VALUE name)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    (void) ProfileImage(image, STRING_PTR(name), NULL, 0, MagickTrue);
    rm_check_image_exception(image, RetainOnError);

    return self;
}


/*
    Method:     Image#despeckle
    Purpose:    reduces the speckle noise in an image while preserving the
                edges of the original image
    Returns:    a new image
*/
VALUE
Image_despeckle(VALUE self)
{
    Image *image, *new_image;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = DespeckleImage(image, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Method:     Image#difference
    Purpose:    Call the IsImagesEqual function
    Returns:    An array with 3 values:
                [0] mean error per pixel
                [1] normalized mean error
                [2] normalized maximum error
    Notes:      "other" can be either an Image or an Image
*/
VALUE Image_difference(VALUE self, VALUE other)
{
    Image *image;
    Image *image2;
    volatile VALUE mean, nmean, nmax;

    Data_Get_Struct(self, Image, image);
    Data_Get_Struct(ImageList_cur_image(other), Image, image2);

    (void) IsImagesEqual(image, image2);
    // No need to check for error

    mean  = rb_float_new(image->error.mean_error_per_pixel);
    nmean = rb_float_new(image->error.normalized_mean_error);
    nmax  = rb_float_new(image->error.normalized_maximum_error);
    return rb_ary_new3(3, mean, nmean, nmax);
}


DEF_ATTR_READER(Image, directory, str)


/*
    Method:     Image#displace(displacement_map, x_amp, y_amp, x_offset=0, y_offset=0)
                Image#displace(displacement_map, x_amp, y_amp, gravity, x_offset=0, y_offset=0)
    Purpose:    Implement the -displace option of xMagick's composite command
    Notes:      If y_amp is omitted the default is x_amp.
*/
VALUE
Image_displace(int argc, VALUE *argv, VALUE self)
{

    Image *image, *displacement_map;
    double x_amplitude = 0.0, y_amplitude = 0.0;
    long x_offset = 0L, y_offset = 0L;

    Data_Get_Struct(self, Image, image);

    if (argc < 2)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 to 6)", argc);
    }

    if (argc > 3)
    {
        Data_Get_Struct(ImageList_cur_image(argv[0]), Image, displacement_map);
        get_composite_offsets(argc-3, &argv[3], image, displacement_map, &x_offset, &y_offset);
        // There must be 3 arguments left
        argc = 3;
    }

    switch (argc)
    {
        case 3:
            y_amplitude = NUM2DBL(argv[2]);
            x_amplitude = NUM2DBL(argv[1]);
            break;
        case 2:
            x_amplitude = NUM2DBL(argv[1]);
            y_amplitude = x_amplitude;
            break;
    }

    Data_Get_Struct(ImageList_cur_image(argv[0]), Image, displacement_map);
    return special_composite(image, displacement_map, x_amplitude, y_amplitude
                           , x_offset, y_offset, DisplaceCompositeOp);
}


/*
    Method:     Image#dispatch(x, y, columns, rows, map <, float>)
    Purpose:    Extracts pixel data from the image and returns it as an
                array of pixels. The "x", "y", "width" and "height" parameters
                specify the rectangle to be extracted. The "map" parameter
                reflects the expected ordering of the pixel array. It can be
                any combination or order of R = red, G = green, B = blue, A =
                alpha, C = cyan, Y = yellow, M = magenta, K = black, or I =
                intensity (for grayscale). If the "float" parameter is specified
                and true, the pixel data is returned as floating-point numbers
                in the range [0..1]. By default the pixel data is returned as
                integers in the range [0..MaxRGB].
    Returns:    an Array
    Raises:     ArgumentError
*/
VALUE
Image_dispatch(int argc, VALUE *argv, VALUE self)
{
    Image *image;
    long x, y;
    unsigned long columns, rows, n, npixels;
    volatile VALUE pixels_ary;
    StorageType stg_type = FIX_STG_TYPE;
    char *map;
    long mapL;
    MagickBooleanType okay;
    ExceptionInfo exception;
    union
    {
        Quantum *i;
        double *f;
        void *v;
    } pixels;

    if (argc < 5 || argc > 6)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 5 or 6)", argc);
    }

    x       = NUM2LONG(argv[0]);
    y       = NUM2LONG(argv[1]);
    columns = NUM2ULONG(argv[2]);
    rows    = NUM2ULONG(argv[3]);
    map     = STRING_PTR_LEN(argv[4], mapL);
    if (argc == 6)
    {
        stg_type = RTEST(argv[5]) ? DoublePixel : FIX_STG_TYPE;
    }

    // Compute the size of the pixel array and allocate the memory.
    npixels = columns * rows * mapL;
    pixels.v = stg_type == FIX_STG_TYPE ? (void *) ALLOC_N(Quantum, npixels)
                                        : (void *) ALLOC_N(double, npixels);

    // Create the Ruby array for the pixels. Return this even if DispatchImage fails.
    pixels_ary = rb_ary_new();

    Data_Get_Struct(self, Image, image);

    GetExceptionInfo(&exception);
    okay =
#if defined(HAVE_EXPORTIMAGEPIXELS)
           ExportImagePixels
#else
           DispatchImage
#endif
                        (image, x, y, columns, rows, map, stg_type, pixels.v, &exception);

    if (!okay)
    {
        goto exit;
    }

    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    // Convert the pixel data to the appropriate Ruby type
    if (stg_type == FIX_STG_TYPE)
    {
        for (n = 0; n < npixels; n++)
        {
            (void) rb_ary_push(pixels_ary, UINT2NUM((unsigned int) pixels.i[n]));
        }
    }
    else
    {
        for (n = 0; n < npixels; n++)
        {
            (void) rb_ary_push(pixels_ary, rb_float_new((double)pixels.f[n]));
        }
    }

exit:
    xfree(pixels.v);
    return pixels_ary;
}

/*
    Method:     Image#display
    Purpose:    display the image to an X window screen
*/
VALUE Image_display(VALUE self)
{
    Image *image;
    Info *info;
    volatile VALUE info_obj;

    Data_Get_Struct(self, Image, image);
    if (image->rows == 0 || image->columns == 0)
    {
        rb_raise(rb_eArgError, "invalid image geometry (%lux%lu)", image->rows, image->columns);
    }

    info_obj = rm_info_new();
    Data_Get_Struct(info_obj, Info, info);

    (void) DisplayImages(info, image);
    rm_check_image_exception(image, RetainOnError);

    return self;
}

/*
    Method:     Image#dispose
    Purpose:    Return the dispose attribute as a DisposeType enum
*/
VALUE
Image_dispose(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return DisposeType_new(image->dispose);
}

/*
    Method:     Image#dispose=
    Purpose:    Set the dispose attribute
*/
VALUE
Image_dispose_eq(VALUE self, VALUE dispose)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    VALUE_TO_ENUM(dispose, image->dispose, DisposeType);
    return self;
}



#if defined(GRAPHICSMAGICK)
/*
    Static:     create_mattes
    Purpose:    GraphicsMagick establishes the source image mattes in
                command.c, before calling CompositeImage. This function does
                that step for Image_dissolve when we're built for GraphicsMagick.
*/
static void
create_mattes(Image *image, double src_percent)
{
    long x, y;
    PixelPacket *q;

    if (!image->matte)
    {
        SetImageOpacity(image,OpaqueOpacity);
    }

    for (y = 0; y < (long) image->rows; y++)
    {
      q = GetImagePixels(image, 0, y, image->columns, 1);

      if (q == NULL)
      {
          break;
      }

      for (x = 0; x < (long) image->columns; x++)
      {
        q->opacity = (Quantum) (((MaxRGB - q->opacity) * src_percent) / 100.0);
        q += 1;
      }

      if (!SyncImagePixels(image))
      {
          break;
      }
    }
}
#endif

/*
    Method:     Image#dissolve(overlay, src_percent, dst_percent, x_offset=0, y_offset=0)
                Image#dissolve(overlay, src_percent, dst_percent, gravity, x_offset=0, y_offset=0)
    Purpose:    Corresponds to the composite -dissolve operation
    Notes:      `percent' can be a number or a string in the form "NN%"
                The "default" value of dst_percent is -1.0, which tells
                blend_geometry to leave it out of the geometry string.
*/
VALUE
Image_dissolve(int argc, VALUE *argv, VALUE self)
{
    Image *image, *overlay;
    double src_percent, dst_percent = -1.0;
    long x_offset = 0L, y_offset = 0L;
    volatile VALUE composite;

    Data_Get_Struct(self, Image, image);

    if (argc < 1)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 to 6)", argc);
    }

    if (argc > 3)
    {
        Data_Get_Struct(ImageList_cur_image(argv[0]), Image, overlay);
        get_composite_offsets(argc-3, &argv[3], image, overlay, &x_offset, &y_offset);
        // There must be 3 arguments left
        argc = 3;
    }

    switch (argc)
    {
        case 3:
            dst_percent = rm_percentage(argv[2]) * 100.0;
        case 2:
            src_percent = rm_percentage(argv[1]) * 100.0;
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 to 6)", argc);
            break;
    }

    Data_Get_Struct(ImageList_cur_image(argv[0]), Image, overlay);

    // GraphicsMagick needs an extra step (ref: GM's command.c)
#if defined(GRAPHICSMAGICK)
    overlay = rm_clone_image(overlay);
    create_mattes(overlay, src_percent);
#endif

    composite =  special_composite(image, overlay, src_percent, dst_percent
                                 , x_offset, y_offset, DissolveCompositeOp);

#if defined(GRAPHICSMAGICK)
    (void) DestroyImage(overlay);
#endif

    return composite;
}


/*
 *  Method:     Image#distortion_channel(reconstructed_image, metric[, channel...])
 *  Purpose:    Call GetImageChannelDistortion
*/
VALUE
Image_distortion_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_GETIMAGECHANNELDISTORTION)
    Image *image, *reconstruct;
    ChannelType channels;
    ExceptionInfo exception;
    MetricType metric;
    double distortion;

    Data_Get_Struct(self, Image, image);

    channels = extract_channels(&argc, argv);
    if (argc > 2)
    {
        raise_ChannelType_error(argv[argc-1]);
    }
    if (argc < 2)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 or more)", argc);
    }

    Data_Get_Struct(ImageList_cur_image(argv[0]), Image, reconstruct);
    VALUE_TO_ENUM(argv[1], metric, MetricType);
    GetExceptionInfo(&exception);
    (void) GetImageChannelDistortion(image, reconstruct, channels
                                   , metric, &distortion, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return rb_float_new(distortion);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:     Image#_dump(aDepth)
    Purpose:    implement marshalling
    Returns:    a string representing the dumped image
    Notes:      uses ImageToBlob - use the MIFF format
                in the blob since it's the most general
*/
VALUE
Image__dump(VALUE self, VALUE depth)
{
    Image *image;
    ImageInfo *info;
    void *blob;
    size_t length;
    DumpedImage mi;
    volatile VALUE str;
    ExceptionInfo exception;

    depth = depth;  // Suppress "never referenced" message from icc

    Data_Get_Struct(self, Image, image);

    info = CloneImageInfo(NULL);
    if (!info)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }
    strcpy(info->magick, image->magick);

    GetExceptionInfo(&exception);
    blob = ImageToBlob(info, image, &length, &exception);

    // Free ImageInfo first - error handling may raise an exception
    (void) DestroyImageInfo(info);

    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    if (!blob)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }

    // Create a header for the blob: ID and version
    // numbers, followed by the length of the magick
    // string stored as a byte, followed by the
    // magick string itself.
    mi.id = DUMPED_IMAGE_ID;
    mi.mj = DUMPED_IMAGE_MAJOR_VERS;
    mi.mi = DUMPED_IMAGE_MINOR_VERS;
    strcpy(mi.magick, image->magick);
    mi.len = (unsigned char) min((size_t)UCHAR_MAX, strlen(mi.magick));

    // Concatenate the blob onto the header & return the result
    str = rb_str_new((char *)&mi, (long)(mi.len+offsetof(DumpedImage,magick)));
    return rb_str_cat(str, (char *)blob, (long)length);
}

/*
    Method:     Image#dup
    Purpose:    Construct a new image object and call initialize_copy
*/
VALUE
Image_dup(VALUE self)
{
    volatile VALUE dup;

    dup = Data_Wrap_Struct(CLASS_OF(self), NULL, DestroyImage, NULL);
    if (rb_obj_tainted(self))
    {
        (void) rb_obj_taint(dup);
    }
    return rb_funcall(dup, rm_ID_initialize_copy, 1, self);
}

/*
    Method:  Image#each_profile
    Purpose: Iterate over image profiles
    Notes:   ImageMagick only
*/
VALUE
Image_each_profile(VALUE self)
{
#if defined(HAVE_GETNEXTIMAGEPROFILE)
    Image *image;
    volatile VALUE ary, val;
    char *name;

    Data_Get_Struct(self, Image, image);

    ResetImageProfileIterator(image);

    ary = rb_ary_new2(2);

    name = GetNextImageProfile(image);
    while (name)
    {
        rb_ary_store(ary, 0, rb_str_new2(name));
#if defined(HAVE_ACQUIRESTRINGINFO)
        {
            const StringInfo *profile;

            profile = GetImageProfile(image, name);
            if (!profile)
            {
                rb_ary_store(ary, 1, Qnil);
            }
            else
            {
                rb_ary_store(ary, 1, rb_str_new((char *)profile->datum, (long)profile->length));
            }
        }
#else
        {
            unsigned char *profile;
            size_t length;

            profile = GetImageProfile(image, "iptc", &length);
            if (!profile)
            {
                rb_ary_store(ary, 1, Qnil);
            }
            else
            {
                rb_ary_store(ary, 1, rb_string_new((char *)profile, (long)length));
            }
        }
#endif
        val = rb_yield(ary);
        name = GetNextImageProfile(image);
    }

    return val;
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:     Image#edge(radius=0)
    Purpose:    finds edges in an image. "radius" defines the radius of the
                convolution filter. Use a radius of 0 and edge selects a
                suitable radius
    Returns:    a new image
*/
VALUE
Image_edge(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double radius = 0.0;
    ExceptionInfo exception;

    switch (argc)
    {
        case 1:
            radius = NUM2DBL(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 or 1)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = EdgeImage(image, radius, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Static:     effect_image
    Purpose:    call one of the effects methods
*/
static VALUE
effect_image(
    VALUE self,
    int argc,
    VALUE *argv,
    effector_t effector)
{
    Image *image, *new_image;
    ExceptionInfo exception;
    double radius = 0.0, sigma = 1.0;

    switch (argc)
    {
        case 2:
            sigma = NUM2DBL(argv[1]);
        case 1:
            radius = NUM2DBL(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 2)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);
    if (sigma == 0.0)
    {
        rb_raise(rb_eArgError, "sigma must be != 0.0");
    }

    GetExceptionInfo(&exception);
    new_image = (effector)(image, radius, sigma, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Method:     Image#emboss(radius=0.0, sigma=1.0)
    Purpose:    creates a grayscale image with a three-dimensional effect
*/
VALUE
Image_emboss(int argc, VALUE *argv, VALUE self)
{
    return effect_image(self, argc, argv, EmbossImage);
}



/*
    Method:     Image#endian
    Purpose:    Return endian option for images that support it.
*/
VALUE
Image_endian(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return EndianType_new(image->endian);
}


/*
    Method:     Image#endian=
    Purpose:    Set endian option for images that support it.
*/
VALUE
Image_endian_eq(VALUE self, VALUE type)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    VALUE_TO_ENUM(type, image->endian, EndianType);
    return self;
}

/*
    Method:     Image#enhance
    Purpose:    applies a digital filter that improves the quality of a noisy image
*/
VALUE
Image_enhance(VALUE self)
{
    Image *image, *new_image;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    GetExceptionInfo(&exception);

    new_image = EnhanceImage(image, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Method:     Image#equalize
    Purpose:    applies a histogram equalization to the image
*/
VALUE
Image_equalize(VALUE self)
{
    Image *image, *new_image;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);
    new_image = rm_clone_image(image);

    (void) EqualizeImage(new_image);
    rm_check_image_exception(new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    return rm_image_new(new_image);
}

/*
    Method:     Image#erase!
    Purpose:    reset the image to the background color
    Notes:      one of the very few Image methods that do not
                return a new image.
*/
VALUE
Image_erase_bang(VALUE self)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    (void) SetImageBackgroundColor(image);

    rm_check_image_exception(image, RetainOnError);

    return self;
}

/*
    Method:     Image#export_pixels(x=0, y=0, cols=self.columns, rows=self.rows, map="RGB")
    Purpose:    extract image pixels in the form of an array
*/
VALUE
Image_export_pixels(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_EXPORTIMAGEPIXELS)
    Image *image;
    long x_off = 0L, y_off = 0L;
    unsigned long cols, rows;
    long n, npixels;
    unsigned int okay;
    char *map = "RGB";
    volatile Quantum *pixels;
    volatile VALUE ary;
    ExceptionInfo exception;


    Data_Get_Struct(self, Image, image);
    cols = image->columns;
    rows = image->rows;

    switch (argc)
    {
        case 5:
            map   = STRING_PTR(argv[4]);
        case 4:
            rows  = NUM2ULONG(argv[3]);
        case 3:
            cols  = NUM2ULONG(argv[2]);
        case 2:
            y_off = NUM2LONG(argv[1]);
        case 1:
            x_off = NUM2LONG(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 to 5)", argc);
            break;
    }

    if (   x_off < 0 || (unsigned long)x_off > image->columns
        || y_off < 0 || (unsigned long)y_off > image->rows
        || cols == 0 || rows == 0)
    {
        rb_raise(rb_eArgError, "invalid extract geometry");
    }


    npixels = (long)(cols * rows * strlen(map));
    pixels = ALLOC_N(Quantum, npixels);
    if (!pixels)    // app recovered from exception
    {
        return rb_ary_new2(0L);
    }

    GetExceptionInfo(&exception);

    okay = ExportImagePixels(image, x_off, y_off, cols, rows, map, QuantumPixel, (void *)pixels, &exception);
    if (!okay)
    {
        xfree((void *)pixels);
        CHECK_EXCEPTION()

        // Should never get here...
        rm_magick_error("ExportImagePixels failed with no explanation.", NULL);
    }

    (void) DestroyExceptionInfo(&exception);

    ary = rb_ary_new2(npixels);
    for (n = 0; n < npixels; n++)
    {
        (void) rb_ary_push(ary, UINT2NUM((unsigned int)pixels[n]));
    }

    xfree((Quantum *)pixels);

    return ary;

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:     Image#export_pixels_to_str(x=0, y=0, cols=self.columns,
                       rows=self.rows, map="RGB", type=Magick::CharPixel)
    Purpose:    extract image pixels to a Ruby string
*/
VALUE
Image_export_pixels_to_str(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_EXPORTIMAGEPIXELS)
    Image *image;
    long x_off = 0L, y_off = 0L;
    unsigned long cols, rows;
    unsigned long npixels;
    size_t sz;
    unsigned int okay;
    char *map = "RGB";
    StorageType type = CharPixel;
    volatile VALUE string;
    char *str;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    cols = image->columns;
    rows = image->rows;

    switch (argc)
    {
        case 6:
            VALUE_TO_ENUM(argv[5], type, StorageType);
        case 5:
            map   = STRING_PTR(argv[4]);
        case 4:
            rows  = NUM2ULONG(argv[3]);
        case 3:
            cols  = NUM2ULONG(argv[2]);
        case 2:
            y_off = NUM2LONG(argv[1]);
        case 1:
            x_off = NUM2LONG(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 to 6)", argc);
            break;
    }

    if (   x_off < 0 || (unsigned long)x_off > image->columns
        || y_off < 0 || (unsigned long)y_off > image->rows
        || cols == 0 || rows == 0)
    {
        rb_raise(rb_eArgError, "invalid extract geometry");
    }


    npixels = cols * rows * strlen(map);
    switch (type)
    {
        case CharPixel:
            sz = sizeof(unsigned char);
            break;
        case ShortPixel:
            sz = sizeof(unsigned short);
            break;
        case DoublePixel:
            sz = sizeof(double);
            break;
        case FloatPixel:
            sz = sizeof(float);
            break;
        case IntegerPixel:
            sz = sizeof(unsigned int);
            break;
        case LongPixel:
            sz = sizeof(unsigned long);
            break;
#if defined(HAVE_QUANTUMPIXEL)
        case QuantumPixel:
            sz = sizeof(Quantum);
            break;
#endif
        case UndefinedPixel:
        default:
            rb_raise(rb_eArgError, "undefined storage type");
            break;
    }

    // Allocate a string long enough to hold the exported pixel data.
    // Get a pointer to the buffer.
    string = rb_str_new2("");
    (void) rb_str_resize(string, (long)(sz * npixels));
    str = STRING_PTR(string);

    GetExceptionInfo(&exception);

    okay = ExportImagePixels(image, x_off, y_off, cols, rows, map, type, (void *)str, &exception);
    if (!okay)
    {
        // Let GC have the string buffer.
        (void) rb_str_resize(string, 0);
        CHECK_EXCEPTION()

        // Should never get here...
        rm_magick_error("ExportImagePixels failed with no explanation.", NULL);
    }

    (void) DestroyExceptionInfo(&exception);

    return string;

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:     Image#extract_info, extract_info=
    Purpose:    the extract_info attribute accessors
*/
VALUE
Image_extract_info(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
#ifdef HAVE_IMAGE_EXTRACT_INFO
    return Rectangle_from_RectangleInfo(&image->extract_info);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

VALUE
Image_extract_info_eq(VALUE self, VALUE rect)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
#ifdef HAVE_IMAGE_EXTRACT_INFO
    Rectangle_to_RectangleInfo(&image->extract_info, rect);
    return self;
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


DEF_ATTR_READER(Image, filename, str)


/*
    Method:     Image#filesize
    Purpose:    Return the image filesize
*/
VALUE Image_filesize(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return INT2FIX(GetBlobSize(image));
}


/*
    Method:     Image#filter, filter=
    Purpose:    Get/set filter type
*/
VALUE
Image_filter(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return FilterTypes_new(image->filter);
}

VALUE
Image_filter_eq(VALUE self, VALUE filter)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    VALUE_TO_ENUM(filter, image->filter, FilterTypes);
    return self;
}


/*
 *  Method:     Image#find_similar_region(target, x=0, y=0)
 *  Purpose:    Search for a region in the image that is "similar" to the
 *              target image.
 */
VALUE
Image_find_similar_region(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_ISIMAGESIMILAR)
    Image *image, *target;
    volatile VALUE region;
    long x = 0L, y = 0L;
    ExceptionInfo exception;
    unsigned int okay;

    Data_Get_Struct(self, Image, image);

    switch (argc)
    {
        case 3:
            y = NUM2LONG(argv[2]);
        case 2:
            x = NUM2LONG(argv[1]);
        case 1:
            Data_Get_Struct(ImageList_cur_image(argv[0]), Image, target);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 to 3)", argc);
            break;
    }

    GetExceptionInfo(&exception);
    okay = IsImageSimilar(image, target, &x, &y, &exception);
    CHECK_EXCEPTION();
    (void) DestroyExceptionInfo(&exception);

    if (!okay)
    {
        return Qnil;
    }

    region = rb_ary_new2(2);
    rb_ary_store(region, 0L, LONG2NUM(x));
    rb_ary_store(region, 1L, LONG2NUM(y));

    return region;

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#flip
                Image#flip!
    Purpose:    creates a vertical mirror image by reflecting the pixels around
                the central x-axis
    Returns:    flip: a new, flipped image
                flip!: self, flipped
*/

static VALUE
flipflop(int bang, VALUE self, flipper_t flipflopper)
{
    Image *image, *new_image;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = (flipflopper)(image, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    if (bang)
    {
        DATA_PTR(self) = new_image;
        (void) DestroyImage(image);
        return self;
    }

    return rm_image_new(new_image);
}

VALUE
Image_flip(VALUE self)
{
    return flipflop(False, self, FlipImage);
}

VALUE
Image_flip_bang(VALUE self)
{
    rm_check_frozen(self);
    return flipflop(True, self, FlipImage);
}

/*
    Method:     Image#flop
                Image#flop!
    Purpose:    creates a horizontal mirror image by reflecting the pixels around
                the central y-axis
    Returns:    flop: a new, flopped image
                flop!: self, flopped
*/
VALUE
Image_flop(VALUE self)
{
    return flipflop(False, self, FlopImage);
}

VALUE
Image_flop_bang(VALUE self)
{
    rm_check_frozen(self);
    return flipflop(True, self, FlopImage);
}


/*
    Method:     Image#format
    Purpose:    Return the image encoding format
    Note:       This is what PerlMagick does for "format"
*/
VALUE
Image_format(VALUE self)
{
    Image *image;
    const MagickInfo *magick_info;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    if (*image->magick)
    {
        // Deliberately ignore the exception info!
        GetExceptionInfo(&exception);
        magick_info = GetMagickInfo(image->magick, &exception);
        (void) DestroyExceptionInfo(&exception);
        return magick_info ? rb_str_new2(magick_info->name) : Qnil;
    }

    return Qnil;
}


/*
    Method:     Image#format=
    Purpose:    Set the image encoding format
*/
VALUE
Image_format_eq(VALUE self, VALUE magick)
{
    Image *image;
    const MagickInfo *m;
    char *mgk;
    ExceptionInfo exception;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    GetExceptionInfo(&exception);

    mgk = STRING_PTR(magick);
    m = GetMagickInfo(mgk, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    if (!m)
    {
        rb_raise(rb_eArgError, "unknown format: %s", mgk);
    }


    strncpy(image->magick, m->name, MaxTextExtent-1);
    return self;
}


/*
    Method:     Image#frame(<width<, height<, x<, y<, inner_bevel<, outer_bevel
                                                                <, color>>>>>>>)
    Purpose:    adds a simulated three-dimensional border around the image.
                "Width" and "height" specify the width and height of the frame.
                The "x" and "y" arguments position the image within the frame.
                If the image is supposed to be centered in the frame, x and y
                should be 1/2 the width and height of the frame. (I.e. if the
                frame is 50 pixels high and 50 pixels wide, x and y should both
                be 25)."Inner_bevel" and "outer_bevel" indicate the width of the
                inner and outer shadows of the frame. They should be much
                smaller than the frame and cannot be > 1/2 the frame width or
                height of the image.
    Default:    All arguments are optional. The defaults are the same as they
                are in Magick++:

                width:  image-columns+25*2
                height: image-rows+25*2
                x:      25
                y:      25
                inner:  6
                outer:  6
                color:  image matte_color (which defaults to #bdbdbd, whatever
                        self.matte_color was set to when the image was created,
                        or whatever image.matte_color is currently set to)

    Returns:    a new image.
*/
VALUE
Image_frame(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    ExceptionInfo exception;
    FrameInfo frame_info;

    Data_Get_Struct(self, Image, image);

    frame_info.width = image->columns + 50;
    frame_info.height = image->rows + 50;
    frame_info.x = 25;
    frame_info.y = 25;
    frame_info.inner_bevel = 6;
    frame_info.outer_bevel = 6;

    switch (argc)
    {
        case 7:
            Color_to_PixelPacket(&image->matte_color, argv[6]);
        case 6:
            frame_info.outer_bevel = NUM2LONG(argv[5]);
        case 5:
            frame_info.inner_bevel = NUM2LONG(argv[4]);
        case 4:
            frame_info.y = NUM2LONG(argv[3]);
        case 3:
            frame_info.x = NUM2LONG(argv[2]);
        case 2:
            frame_info.height = image->rows + 2*NUM2LONG(argv[1]);
        case 1:
            frame_info.width = image->columns + 2*NUM2LONG(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 7)", argc);
            break;
    }

    GetExceptionInfo(&exception);
    new_image = FrameImage(image, &frame_info, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Method:     Image.from_blob(blob) <{ parm block }>
    Purpose:    Call BlobToImage
    Notes:      The blob is a String
*/
VALUE
Image_from_blob(VALUE class, VALUE blob_arg)
{
    Image *images;
    Info *info;
    volatile VALUE info_obj;
    ExceptionInfo exception;
    void *blob;
    long length;

    class = class;      // defeat gcc message
    blob = (void *) STRING_PTR_LEN(blob_arg, length);

    // Get a new Info object - run the parm block if supplied
    info_obj = rm_info_new();
    Data_Get_Struct(info_obj, Info, info);

    GetExceptionInfo(&exception);
    images = BlobToImage(info,  blob, (size_t)length, &exception);
    rm_check_exception(&exception, images, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(images);

    return array_from_images(images);
}

DEF_ATTR_READER(Image, fuzz, dbl)

/*
    Method:     Image#fuzz=number
                Image#fuzz=NN%
    Notes:      See Info#fuzz.
*/
VALUE Image_fuzz_eq(VALUE self, VALUE fuzz)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    image->fuzz = rm_fuzz_to_dbl(fuzz);
    return self;
}


DEF_ATTR_ACCESSOR(Image, gamma, dbl)


/*
 *  Method:     Image#gamma_channel(gamma, channel=AllChannels)
 *  Returns     a new image
*/
VALUE
Image_gamma_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_GAMMAIMAGECHANNEL)
    Image *image, *new_image;
    ChannelType channels;

    channels = extract_channels(&argc, argv);

    // There must be exactly one remaining argument.
    if (argc == 0)
    {
        rb_raise(rb_eArgError, "missing gamma argument");
    }
    else if (argc > 1)
    {
        raise_ChannelType_error(argv[argc-1]);
    }

    Data_Get_Struct(self, Image, image);
    new_image = rm_clone_image(image);

    (void)GammaImageChannel(new_image, channels, NUM2DBL(argv[0]));
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#gamma_correct(red_gamma<, green_gamma<, blue_gamma>>>)
    Purpose:    gamma-correct an image
    Notes:      At least red_gamma must be specified. If one or more levels are
                omitted, the last specified number is used as the default.
                For backward compatibility accept a 4th argument but ignore it.
*/
VALUE
Image_gamma_correct(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double red_gamma, green_gamma, blue_gamma;
    char gamma[50];

    switch(argc)
    {
        case 1:
            red_gamma   = NUM2DBL(argv[0]);

            // Can't have all 4 gamma values == 1.0. Also, very small values
            // cause ImageMagick to segv.
            if (red_gamma == 1.0 || fabs(red_gamma) < 0.003)
            {
                rb_raise(rb_eArgError, "invalid gamma value (%f)", red_gamma);
            }
            green_gamma = blue_gamma = red_gamma;
            break;
        case 2:
            red_gamma   = NUM2DBL(argv[0]);
            green_gamma = NUM2DBL(argv[1]);
            blue_gamma  = green_gamma;
            break;
        case 3:
        case 4:
            red_gamma     = NUM2DBL(argv[0]);
            green_gamma   = NUM2DBL(argv[1]);
            blue_gamma    = NUM2DBL(argv[2]);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 to 3)", argc);
            break;
    }

    sprintf(gamma, "%f,%f,%f", red_gamma, green_gamma, blue_gamma);
    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    (void) GammaImage(new_image, gamma);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}

/*
    Method:     Image#gaussian_blur(radius, sigma)
    Purpose:    blur the image
    Returns:    a new image
*/
VALUE
Image_gaussian_blur(int argc, VALUE *argv, VALUE self)
{
    return effect_image(self, argc, argv, GaussianBlurImage);
}


/*
 *  Method:     Image#gaussian_blur_channel(radius=0, sigma=1, channel=AllChannels)
 *  Notes:      new in IM 6.0.0
*/
VALUE
Image_gaussian_blur_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_GAUSSIANBLURIMAGECHANNEL)
    Image *image, *new_image;
    ChannelType channels;
    ExceptionInfo exception;
    double radius = 0.0, sigma = 1.0;

    channels = extract_channels(&argc, argv);

    // There can be 0, 1, or 2 remaining arguments.
    switch(argc)
    {
        case 2:
            sigma = NUM2DBL(argv[1]);
            /* Fall thru */
        case 1:
            radius = NUM2DBL(argv[0]);
            /* Fall thru */
        case 0:
            break;
        default:
            raise_ChannelType_error(argv[argc-1]);
    }

    GetExceptionInfo(&exception);

    Data_Get_Struct(self, Image, image);
    new_image = GaussianBlurImageChannel(image, channels, radius, sigma, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#geometry, geometry=
    Purpose:    the preferred size of the image when encoding
*/
DEF_ATTR_READER(Image, geometry, str)

VALUE
Image_geometry_eq(
    VALUE self,
    VALUE geometry)
{
    Image *image;
    volatile VALUE geom_str;
    char *geom;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    if (geometry == Qnil)
    {
        magick_free(image->geometry);
        image->geometry = NULL;
        return self;
    }


    geom_str = rb_funcall(geometry, rm_ID_to_s, 0);
    geom = STRING_PTR(geom_str);
    if (!IsGeometry(geom))
    {
        rb_raise(rb_eTypeError, "invalid geometry: %s", geom);
    }
    magick_clone_string(&image->geometry, geom);
    return self;
}


/*
    Method:     Image#get_pixels(x, y, columns. rows)
    Purpose:    Call AcquireImagePixels
    Returns:    An array of Magick::Pixel objects corresponding to the pixels in
                the rectangle defined by the geometry parameters.
    Note:       This is the complement of store_pixels. Notice that the
                return value is an array object even when only one pixel is
                returned.
                store_pixels calls GetImagePixels, then SyncImage
*/
VALUE
Image_get_pixels(
    VALUE self,
    VALUE x_arg,
    VALUE y_arg,
    VALUE cols_arg,
    VALUE rows_arg)
{
    Image *image;
    const PixelPacket *pixels;
    ExceptionInfo exception;
    long x, y;
    unsigned long columns, rows;
    long size, n;
    VALUE pixel_ary;

    Data_Get_Struct(self, Image, image);
    x       = NUM2LONG(x_arg);
    y       = NUM2LONG(y_arg);
    columns = NUM2ULONG(cols_arg);
    rows    = NUM2ULONG(rows_arg);

    if ((x+columns) > image->columns || (y+rows) > image->rows)
    {
        rb_raise(rb_eRangeError, "geometry (%lux%lu%+ld%+ld) exceeds image bounds"
               , columns, rows, x, y);
    }

    // Cast AcquireImagePixels to get rid of the const qualifier. We're not going
    // to change the pixels but I don't want to make "pixels" const.
    GetExceptionInfo(&exception);
    pixels = AcquireImagePixels(image, x, y, columns, rows, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    // If the function failed, return a 0-length array.
    if (!pixels)
    {
        return rb_ary_new();
    }

    // Allocate an array big enough to contain the PixelPackets.
    size = (long)(columns * rows);
    pixel_ary = rb_ary_new2(size);

    // Convert the PixelPackets to Magick::Pixel objects
    for (n = 0; n < size; n++)
    {
        rb_ary_store(pixel_ary, n, Pixel_from_PixelPacket(&pixels[n]));
    }

    return pixel_ary;
}

#if 0

Removed: given Image#pixel_color, this is redundant
/*
    Method:     Image#get_one_pixel
    Purpose:    Call AcquireOnePixel
    Returns:    the x,y pixel as a Magick::Pixel
    See also:   pixel_color
*/
VALUE
Image_get_one_pixel(VALUE self, VALUE x, VALUE y)
{
    Image *image;
    PixelPacket pixel;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);
    pixel = AcquireOnePixel(image, NUM2LONG(x), NUM2LONG(y), &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return Pixel_from_PixelPacket(&pixel);
}
#endif

/*
    Method:     Image#gray?
    Purpose:    return true if all the pixels in the image have the same red,
                green, and blue intensities.
*/
VALUE
Image_gray_q(VALUE self)
{
    Image *image;
    ExceptionInfo exception;
    unsigned int r;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    r = IsGrayImage(image, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return r ? Qtrue : Qfalse;
}

/*
    Method:     Image#grayscale_pseudo_class
    Purpose:    Call GrayscalePseudoClassImage
*/
VALUE
Image_grayscale_pseudo_class(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_GRAYSCALEPSEUDOCLASSIMAGE)
    Image *image, *new_image;
    unsigned int optimize = True;

    switch(argc)
    {
        case 1:
           optimize = RTEST(argv[0]);
           /* Fall thru */
        case 0:
           break;
        default:
           rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 or 1)", argc);
    }

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    GrayscalePseudoClassImage(new_image, optimize);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif

}


/*
    Method:     Image#implode(amount=0.50)
    Purpose:    implode the image by the specified percentage
    Returns:    a new image
*/
VALUE
Image_implode(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double amount = 0.50;
    ExceptionInfo exception;

    switch (argc)
    {
        case 1:
            amount = NUM2DBL(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 or 1)", argc);
    }

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = ImplodeImage(image, amount, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);
    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}


/*
    Method:     Image#import_pixels
    Purpose:    store image pixel data from an array
    Notes:      See Image#export_pixels
*/
VALUE
Image_import_pixels(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_IMPORTIMAGEPIXELS)
    Image *image;
    long x_off, y_off;
    unsigned long cols, rows;
    unsigned long n, npixels;
    long buffer_l;
    char *map;
    volatile VALUE pixel_arg, pixel_ary;
    StorageType stg_type = CharPixel;
    size_t type_sz, map_l;
    volatile Quantum *pixels = NULL;
    volatile double *fpixels = NULL;
    volatile void *buffer;
    unsigned int okay;

    rm_check_frozen(self);

    switch (argc)
    {
        case 7:
            VALUE_TO_ENUM(argv[6], stg_type, StorageType);
        case 6:
            x_off = NUM2LONG(argv[0]);
            y_off = NUM2LONG(argv[1]);
            cols = NUM2ULONG(argv[2]);
            rows = NUM2ULONG(argv[3]);
            map = STRING_PTR(argv[4]);
            pixel_arg = argv[5];
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 6 or 7)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);

    if (x_off < 0 || y_off < 0 || cols <= 0 || rows <= 0)
    {
        rb_raise(rb_eArgError, "invalid import geometry");
    }

    map_l = strlen(map);
    npixels = cols * rows * map_l;

    // Assume that any object that responds to :to_str is a string buffer containing
    // binary pixel data.
    if (rb_respond_to(pixel_arg, rb_intern("to_str")))
    {
        buffer = (void *)STRING_PTR_LEN(pixel_arg, buffer_l);
        switch (stg_type)
        {
            case CharPixel:
                type_sz = 1;
                break;
            case ShortPixel:
                type_sz = sizeof(unsigned short);
                break;
            case IntegerPixel:
                type_sz = sizeof(unsigned int);
                break;
            case LongPixel:
                type_sz = sizeof(unsigned long);
                break;
            case DoublePixel:
                type_sz = sizeof(double);
                break;
            case FloatPixel:
                type_sz = sizeof(float);
                break;
#if defined(HAVE_QUANTUMPIXEL)
            case QuantumPixel:
                type_sz = sizeof(Quantum);
                break;
#endif
            default:
                rb_raise(rb_eArgError, "unsupported storage type %s", StorageType_name(stg_type));
                break;
        }

        if (buffer_l % type_sz != 0)
        {
            rb_raise(rb_eArgError, "pixel buffer must be an exact multiple of the storage type size");
        }
        if ((buffer_l / type_sz) % map_l != 0)
        {
            rb_raise(rb_eArgError, "pixel buffer must contain an exact multiple of the map length");
        }
        if ((unsigned long)(buffer_l / type_sz) < npixels)
        {
            rb_raise(rb_eArgError, "pixel buffer too small (need %lu channel values, got %ld)"
                   , npixels, buffer_l/type_sz);
        }
    }
    // Otherwise convert the argument to an array and convert the array elements
    // to binary pixel data.
    else
    {
        // rb_Array converts an object that is not an array to an array if possible,
        // and raises TypeError if it can't. It usually is possible.
        pixel_ary = rb_Array(pixel_arg);

        if (RARRAY(pixel_ary)->len % map_l != 0)
        {
            rb_raise(rb_eArgError, "pixel array must contain an exact multiple of the map length");
        }
        if ((unsigned long)RARRAY(pixel_ary)->len < npixels)
        {
            rb_raise(rb_eArgError, "pixel array too small (need %lu elements, got %ld)"
                   , npixels, RARRAY(pixel_ary)->len);
        }

        if (stg_type == DoublePixel || stg_type == FloatPixel)
        {
            // Get an array for double pixels. Use Ruby's memory so GC will clean up after
            // us in case of an exception.
            fpixels = ALLOC_N(double, npixels);
            for (n = 0; n < npixels; n++)
            {
                fpixels[n] = NUM2DBL(rb_ary_entry(pixel_ary, n));
            }
            buffer = (void *) fpixels;
            stg_type = DoublePixel;
        }
        else
        {
            // Get array for Quantum pixels. Use Ruby's memory so GC will clean up after us
            // in case of an exception.
            pixels = ALLOC_N(Quantum, npixels);
            for (n = 0; n < npixels; n++)
            {
                volatile VALUE p = rb_ary_entry(pixel_ary, n);
                unsigned long q = (Quantum)NUM2UINT(p);
                pixels[n] = (int) q;
            }
            buffer = (void *) pixels;
            stg_type = QuantumPixel;
        }
    }


    okay = ImportImagePixels(image, x_off, y_off, cols, rows, map, stg_type, (const void *)buffer);

    // Free pixel array before checking for errors.
    if (pixels)
    {
        xfree((void *)pixels);
    }
    if (fpixels)
    {
        xfree((void *)fpixels);
    }

    if (!okay)
    {
        rm_check_image_exception(image, RetainOnError);
        // Shouldn't get here...
        rm_magick_error("ImportImagePixels failed with no explanation.", NULL);
    }

    return self;

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:     Image#inspect
    Purpose:    Overrides Object#inspect - returns a string description of the
                image.
    Returns:    the string
    Notes:      this is essentially the DescribeImage except the description is
                built in a char buffer instead of being written to a file.
*/
VALUE
Image_inspect(VALUE self)
{
    Image *image;
    unsigned long quantum_depth;
    int x = 0;                  // # bytes used in buffer
    char buffer[2048];          // image description buffer

    Data_Get_Struct(self, Image, image);

    // Print magick filename if different from current filename.
    if (*image->magick_filename != '\0' && strcmp(image->magick_filename, image->filename) != 0)
    {
        x += sprintf(buffer+x, "%s=>", image->magick_filename);
    }
    // Print current filename.
    x += sprintf(buffer+x, "%s", image->filename);
    // Print scene number.
    if ((GetPreviousImageInList(image) != NULL) && (GetNextImageInList(image) != NULL) && image->scene > 0)
    {
        x += sprintf(buffer+x, "[%lu]", image->scene);
    }
    // Print format
    x += sprintf(buffer+x, " %s ", image->magick);

    // Print magick columnsXrows if different from current.
    if (image->magick_columns != 0 || image->magick_rows != 0)
    {
        if (image->magick_columns != image->columns || image->magick_rows != image->rows)
        {
            x += sprintf(buffer+x, "%lux%lu=>", image->magick_columns, image->magick_rows);
        }
    }

    x += sprintf(buffer+x, "%lux%lu ", image->columns, image->rows);

    // Print current columnsXrows
    if (   image->page.width != 0 || image->page.height != 0
        || image->page.x != 0     || image->page.y != 0)
    {
        x += sprintf(buffer+x, "%lux%lu%+ld%+ld ", image->page.width, image->page.height
                   , image->page.x, image->page.y);
    }

    if (image->storage_class == DirectClass)
    {
        x += sprintf(buffer+x, "DirectClass ");
        if (image->total_colors != 0)
        {
            if (image->total_colors >= (unsigned long)(1 << 24))
            {
                x += sprintf(buffer+x, "%lumc ", image->total_colors/1024/1024);
            }
            else
            {
                if (image->total_colors >= (unsigned long)(1 << 16))
                {
                    x += sprintf(buffer+x, "%lukc ", image->total_colors/1024);
                }
                else
                {
                    x += sprintf(buffer+x, "%luc ", image->total_colors);
                }
            }
        }
    }
    else
    {
        // Cast `image->colors' to long to suppress gcc warnings when
        // building with GM. GM defines that field as an unsigned int.
        if (image->total_colors <= image->colors)
        {
            x += sprintf(buffer+x, "PseudoClass %ldc ", (long) image->colors);
        }
        else
        {
            x += sprintf(buffer+x, "PseudoClass %lu=>%ldc ", image->total_colors
                       , (long)image->colors);
            if (image->error.mean_error_per_pixel != 0.0)
            {
                 x += sprintf(buffer+x, "%ld/%.6f/%.6fdb "
                            , (long) (image->error.mean_error_per_pixel+0.5)
                            , image->error.normalized_mean_error
                            , image->error.normalized_maximum_error);
            }
        }
    }

    // Print bit depth
#if defined(HAVE_GETIMAGEQUANTUMDEPTH)
#if defined(HAVE_OLD_GETIMAGEQUANTUMDEPTH)
    quantum_depth = GetImageQuantumDepth(image);
#else
    quantum_depth = GetImageQuantumDepth(image, MagickTrue);
#endif
#else
    quantum_depth = image->depth;
#endif
    x += sprintf(buffer+x, "%lu-bit", quantum_depth);

    // Print blob info if appropriate.
    if (GetBlobSize(image) != 0)
    {
        if (GetBlobSize(image) >= (1 << 24))
        {
            x += sprintf(buffer+x, " %lumb", (unsigned long) (GetBlobSize(image)/1024/1024));
        }
        else if (GetBlobSize(image) >= 1024)
        {
            x += sprintf(buffer+x, " %lukb", (unsigned long) (GetBlobSize(image)/1024));
        }
        else
        {
            x += sprintf(buffer+x, " %lub", (unsigned long) GetBlobSize(image));
        }
    }

    assert(x < ((int)sizeof(buffer)-1));
    buffer[x] = '\0';

    return rb_str_new2(buffer);
}


/*
    Method:     Image#interlace
    Purpose:    get the interlace attribute
*/
VALUE
Image_interlace(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);

    return InterlaceType_new(image->interlace);
}


/*
    Method:     Image#interlace=
    Purpose:    set the interlace attribute
*/
VALUE
Image_interlace_eq(VALUE self, VALUE interlace)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    VALUE_TO_ENUM(interlace, image->interlace, InterlaceType);
    return self;
}


/*
    Method:     Image#iptc_profile
    Purpose:    Return the IPTC profile as a String.
    Notes:      If there is no profile, returns Qnil
*/
VALUE
Image_iptc_profile(VALUE self)
{
    Image *image;

#if defined(HAVE_ACQUIRESTRINGINFO)
    const StringInfo *profile;

    Data_Get_Struct(self, Image, image);

    profile = GetImageProfile(image, "iptc");
    if (!profile)
    {
        return Qnil;
    }

    return rb_str_new((char *)profile->datum, (long)profile->length);

#else

    const unsigned char *profile;
    size_t length;

    Data_Get_Struct(self, Image, image);

    profile = GetImageProfile(image, "iptc", &length);
    if (!profile)
    {
        return Qnil;
    }

    return rb_str_new((char *)profile, (long)length);

#endif
}



/*
    Method:     Image#iptc_profile=(String)
    Purpose:    Set the IPTC profile. The argument is a string.
    Notes:      Pass nil to remove any existing profile
*/
VALUE
Image_iptc_profile_eq(VALUE self, VALUE profile)
{
    (void) Image_delete_profile(self, rb_str_new2("IPTC"));
    if (profile != Qnil)
    {
        (void) set_profile(self, "IPTC", profile);
    }
    return self;
}

/*
    These are undocumented methods. The writer is
    called only by Image#iterations=.
    The reader is only used by the unit tests!
*/
DEF_ATTR_ACCESSOR(Image, iterations, int)

/*
    Method:     Image#level(black_point=0.0, white_point=MaxRGB, gamma=1.0)
    Purpose:    adjusts the levels of an image given these points: black, mid, and white
    Notes:      all three arguments are optional
*/
VALUE
Image_level2(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double black_point = 0.0, gamma = 1.0, white_point = (double)MaxRGB;
    char level[50];

    switch(argc)
    {
        case 0:             // take all the defaults
            break;
        case 1:
            black_point = NUM2DBL(argv[0]);
            white_point = MaxRGB - black_point;
            break;
        case 2:
            black_point = NUM2DBL(argv[0]);
            white_point = NUM2DBL(argv[1]);
            break;
        case 3:
            black_point = NUM2DBL(argv[0]);
            white_point = NUM2DBL(argv[1]);
            gamma       = NUM2DBL(argv[2]);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 3)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    sprintf(level, "%gx%g+%g", black_point, white_point, gamma);
    (void) LevelImage(new_image, level);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}

/*
    Method:     Image#level_channel(aChannelType, black=0, white=MaxRGB, gamma=1.0)
    Purpose:    similar to Image#level but applies to a single channel only
    Notes:      black and white are 0-MaxRGB, gamma is 0-10.
*/
VALUE
Image_level_channel(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double black_point = 0.0, gamma = 1.0, white_point = (double)MaxRGB;
    ChannelType channel;

    switch(argc)
    {
        case 1:             // take all the defaults
            break;
        case 2:
            black_point = NUM2DBL(argv[1]);
            white_point = MaxRGB - black_point;
            break;
        case 3:
            black_point = NUM2DBL(argv[1]);
            white_point = NUM2DBL(argv[2]);
            break;
        case 4:
            black_point = NUM2DBL(argv[1]);
            white_point = NUM2DBL(argv[2]);
            gamma       = NUM2DBL(argv[3]);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 to 4)", argc);
            break;
    }

    VALUE_TO_ENUM(argv[0], channel, ChannelType);
    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    (void) LevelImageChannel(new_image, channel, black_point, white_point, gamma);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}

/*
    Method:     Image_linear_stretch(black_point <, white_point>)
    Purpose:    Call LinearStretchImage
    Notes:      The default for white_point is #pixels-black_point.
                See Image_contrast_stretch_channel.
*/
VALUE
Image_linear_stretch(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_LINEARSTRETCHIMAGE)
    Image *image, *new_image;
    double black_point, white_point;

    Data_Get_Struct(self, Image, image);

    get_black_white_point(image, argc, argv, &black_point, &white_point);

    new_image = rm_clone_image(image);

    (void) LinearStretchImage(new_image, black_point, white_point);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:     Image._load
    Purpose:    implement marshalling
    Notes:      calls BlobToImage - see Image#_dump
*/
VALUE
Image__load(VALUE class, VALUE str)
{
    Image *image;
    ImageInfo *info;
    DumpedImage mi;
    ExceptionInfo exception;
    char *blob;
    long length;

    class = class;  // Suppress "never referenced" message from icc

    info = CloneImageInfo(NULL);

    blob = STRING_PTR_LEN(str, length);

    // Must be as least as big as the 1st 4 fields in DumpedImage
    if (length <= (long)sizeof(DumpedImage)-MaxTextExtent)
    {
        rb_raise(rb_eTypeError, "image is invalid or corrupted (too short)");
    }

    // Retrieve & validate the image format from the header portion
    mi.id = ((DumpedImage *)blob)->id;
    if (mi.id != DUMPED_IMAGE_ID)
    {
        rb_raise(rb_eTypeError, "image is invalid or corrupted (invalid header)");
    }

    mi.mj = ((DumpedImage *)blob)->mj;
    mi.mi = ((DumpedImage *)blob)->mi;
    if (   mi.mj != DUMPED_IMAGE_MAJOR_VERS
        || mi.mi > DUMPED_IMAGE_MINOR_VERS)
    {
        rb_raise(rb_eTypeError, "incompatible image format (can't be read)\n"
                                "\tformat version %d.%d required; %d.%d given"
                              , DUMPED_IMAGE_MAJOR_VERS, DUMPED_IMAGE_MINOR_VERS
                              , mi.mj, mi.mi);
    }

    mi.len = ((DumpedImage *)blob)->len;

    // Must be bigger than the header
    if (length <= mi.len+((long)sizeof(DumpedImage))-MaxTextExtent)
    {
        rb_raise(rb_eTypeError, "image is invalid or corrupted (too short)");
    }

    memcpy(info->magick, ((DumpedImage *)blob)->magick, mi.len);
    info->magick[mi.len] = '\0';

    GetExceptionInfo(&exception);

    blob += offsetof(DumpedImage,magick) + mi.len;
    length -= offsetof(DumpedImage,magick) + mi.len;
    image = BlobToImage(info, blob, (size_t) length, &exception);
    (void) DestroyImageInfo(info);

    rm_check_exception(&exception, image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(image);

    return rm_image_new(image);
}


/*
    Method:     Image#magnify
                Image#magnify!
    Purpose:    Scales an image proportionally to twice its size
    Returns:    magnify: a new image 2x the size of the input image
                magnify!: self, 2x

*/
static VALUE
magnify(int bang, VALUE self, magnifier_t magnifier)
{
    Image *image;
    Image *new_image;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = (magnifier)(image, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    if (bang)
    {
        DATA_PTR(self) = new_image;
        (void) DestroyImage(image);
        return self;
    }

    return rm_image_new(new_image);
}

VALUE
Image_magnify(VALUE self)
{
    return magnify(False, self, MagnifyImage);
}

VALUE
Image_magnify_bang(VALUE self)
{
    rm_check_frozen(self);
    return magnify(True, self, MagnifyImage);
}

/*
    Method:     Image#map(map_image, dither=false)
    Purpose:    Call MapImage
    Returns:    a new image
*/

VALUE
Image_map(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    Image *map;
    volatile VALUE map_obj, map_arg;
    unsigned int dither = False;

    switch (argc)
    {
        case 2:
            dither = RTEST(argv[1]);
        case 1:
            map_arg = argv[0];
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 or 2)", argc);
            break;
    }


    Data_Get_Struct(self, Image, image);
    new_image = rm_clone_image(image);

    map_obj = ImageList_cur_image(map_arg);
    Data_Get_Struct(map_obj, Image, map);
    (void) MapImage(new_image, map, dither);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}


/*
    Method:     Image#mask
    Purpose:    Return the image's clip mask, or nil if it doesn't have a clip
                mask.
    Notes:      Distinguish from Image#clip_mask
*/
VALUE
Image_mask(VALUE self)
{
    Image *image, *mask;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    GetExceptionInfo(&exception);

#if defined(HAVE_GETIMAGECLIPMASK)

    // The returned clip mask is a clone, ours to keep.
    mask = GetImageClipMask(image, &exception);
    rm_check_exception(&exception, mask, DestroyOnError);

#else
    mask = image->clip_mask;
#endif

    (void) DestroyExceptionInfo(&exception);

    return mask ? rm_image_new(mask) : Qnil;
}


/*
    Method:     Image#mask=(mask-image)
    Purpose:    associates a clip mask with the image
    Notes:      pass "nil" for the mask-image to remove the current clip mask.
                If the clip mask is not the same size as the target image,
                resizes the clip mask to match the target.
    Notes:      Distinguish from Image#clip_mask=
*/
VALUE
Image_mask_eq(VALUE self, VALUE mask)
{
    Image *image, *mask_image, *resized_image;
    Image *clip_mask;
    long x, y;
    PixelPacket *q;
    ExceptionInfo exception;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    if (mask != Qnil)
    {
        Data_Get_Struct(ImageList_cur_image(mask), Image, mask_image);
        clip_mask = rm_clone_image(mask_image);

        // Resize if necessary
        if (clip_mask->columns != image->columns || clip_mask->rows != image->rows)
        {
            GetExceptionInfo(&exception);
            resized_image = ResizeImage(clip_mask, image->columns, image->rows
                                      , UndefinedFilter, 0.0, &exception);
            rm_check_exception(&exception, resized_image, DestroyOnError);
            (void) DestroyExceptionInfo(&exception);
            rm_ensure_result(resized_image);
            (void) (void) DestroyImage(clip_mask);
            clip_mask = resized_image;
        }

        // The following section is copied from mogrify.c (6.2.8-8)
        for (y = 0; y < (long) clip_mask->rows; y++)
        {
          q = GetImagePixels(clip_mask, 0, y, clip_mask->columns, 1);
          if (!q)
          {
              break;
          }
          for (x = 0; x < (long) clip_mask->columns; x++)
          {
            if (clip_mask->matte == MagickFalse)
            {
                q->opacity = PIXEL_INTENSITY(q);
            }
            q->red = q->opacity;
            q->green = q->opacity;
            q->blue = q->opacity;
            q += 1;
          }
          if (SyncImagePixels(clip_mask) == (MagickBooleanType)False)
          {
              (void) (void) DestroyImage(clip_mask);
              rm_magick_error("SyncImagePixels failed", NULL);
          }
        }

#if defined(HAVE_SETIMAGESTORAGECLASS)
        if (SetImageStorageClass(clip_mask, DirectClass) == (MagickBooleanType)False)
        {
            (void) (void) DestroyImage(clip_mask);
            rm_magick_error("SetImageStorageClass failed", NULL);
        }
#else
        if (clip_mask->storage_class == PseudoClass)
        {
            SyncImage(clip_mask);
            clip_mask->storage_class = DirectClass;
        }
#endif

        clip_mask->matte = MagickTrue;

        // SetImageClipMask clones the clip_mask image. We can
        // destroy our copy after SetImageClipMask is done with it.

        (void) SetImageClipMask(image, clip_mask);
        (void) (void) DestroyImage(clip_mask);
    }
    else
    {
        (void) SetImageClipMask(image, NULL);
    }

    return self;
}


DEF_ATTR_ACCESSOR(Image, matte, bool)

/*
    Method:     Image#matte_color
    Purpose:    Return the matte color
*/
VALUE
Image_matte_color(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return PixelPacket_to_Color_Name(image, &image->matte_color);
}

/*
    Method:     Image#matte_color=
    Purpose:    Set the matte color
*/
VALUE
Image_matte_color_eq(VALUE self, VALUE color)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    Color_to_PixelPacket(&image->matte_color, color);
    return self;
}

/*
    Method:     Image#matte_flood_fill(color, opacity, x, y, method)
    Purpose:    Call MatteFloodFillImage
*/
VALUE
Image_matte_flood_fill(
    VALUE self,
    VALUE color,
    VALUE opacity,
    VALUE x_obj,
    VALUE y_obj,
    VALUE method)
{
    Image *image, *new_image;
    PixelPacket target;
    unsigned long op;
    long x, y;
    PaintMethod pm;

    Data_Get_Struct(self, Image, image);

    Color_to_PixelPacket(&target, color);

    op = NUM2ULONG(opacity);
    if (op > MaxRGB)
    {
        rb_raise(rb_eArgError, "opacity (%lu) exceeds MaxRGB", op);
    }

    VALUE_TO_ENUM(method, pm, PaintMethod);
    if (!(pm == FloodfillMethod || pm == FillToBorderMethod))
    {
        rb_raise(rb_eArgError, "paint method must be FloodfillMethod or "
                               "FillToBorderMethod (%d given)", pm);
    }
    x = NUM2LONG(x_obj);
    y = NUM2LONG(y_obj);
    if ((unsigned long)x > image->columns || (unsigned long)y > image->rows)
    {
        rb_raise(rb_eArgError, "target out of range. %ldx%ld given, image is %lux%lu"
               , x, y, image->columns, image->rows);
    }


    new_image = rm_clone_image(image);

    (void) MatteFloodfillImage(new_image, target, (Quantum)op, x, y, pm);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}

/*
    Method:     Image#median_filter(radius=0.0)
    Purpose:    applies a digital filter that improves the quality of a noisy
                image. Each pixel is replaced by the median in a set of
                neighboring pixels as defined by radius.
    Returns:    a new image
*/
VALUE
Image_median_filter(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double radius = 0.0;
    ExceptionInfo exception;

    switch (argc)
    {
        case 1:
            radius = NUM2DBL(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 or 1)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = MedianFilterImage(image, radius, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

DEF_ATTR_READERF(Image, mean_error_per_pixel, error.mean_error_per_pixel, dbl)

/*
    Method:     Image#mime_type
    Purpose:    Return the officially registered (or de facto) MIME media-type
                corresponding to the image format.
*/
VALUE
Image_mime_type(VALUE self)
{
    Image *image;
    char *type;
    volatile VALUE mime_type;

    Data_Get_Struct(self, Image, image);
    type = MagickToMime(image->magick);
    if (!type)
    {
        return Qnil;
    }
    mime_type = rb_str_new2(type);

    // The returned string must be deallocated by the user.
    magick_free(type);

    return mime_type;
}

/*
    Method:     Image#minify
                Image#minify!
    Purpose:    Scales an image proportionally to half its size
    Returns:    minify: a new image 1/2x the size of the input image
                minify!: self, 1/2x
*/
VALUE
Image_minify(VALUE self)
{
    return magnify(False, self, MinifyImage);
}

VALUE
Image_minify_bang(VALUE self)
{
    rm_check_frozen(self);
    return magnify(True, self, MinifyImage);
}

/*
    Method:     Image#modulate(<brightness<, saturation<, hue>>>)
    Purpose:    control the brightness, saturation, and hue of an image
    Notes:      all three arguments are optional and default to 100%
*/
VALUE
Image_modulate(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double pct_brightness = 100.0,
           pct_saturation = 100.0,
           pct_hue        = 100.0;
    char modulate[100];

    switch (argc)
    {
        case 3:
            pct_hue        = 100*NUM2DBL(argv[2]);
        case 2:
            pct_saturation = 100*NUM2DBL(argv[1]);
        case 1:
            pct_brightness = 100*NUM2DBL(argv[0]);
            break;
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 3)", argc);
            break;
    }

    if (pct_brightness <= 0.0)
    {
        rb_raise(rb_eArgError, "brightness is %g%%, must be positive", pct_brightness);
    }
    sprintf(modulate, "%f%%,%f%%,%f%%", pct_brightness, pct_saturation, pct_hue);
    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    (void) ModulateImage(new_image, modulate);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}

/*
    Method:     Image#monitor= proc
    Purpose:    Establish a progress monitor
    Notes:      A progress monitor is a callable object. Save the monitor proc
                as the client_data and establish `progress_monitor' as the
                monitor exit. When `progress_monitor' is called, retrieve
                the proc and call it.
*/
VALUE
Image_monitor_eq(VALUE self, VALUE monitor)
{
#if defined(HAVE_SETIMAGEPROGRESSMONITOR)
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    if (NIL_P(monitor))
    {
        image->progress_monitor = NULL;
    }
    else
    {
        (void) SetImageProgressMonitor(image, rm_progress_monitor, (void *)monitor);
    }


    return self;
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}




/*
    Method:     Image#monochrome?
    Purpose:    return true if all the pixels in the image have the same red,
                green, and blue intensities and the intensity is either 0 or
                MaxRGB.
*/
VALUE
Image_monochrome_q(VALUE self)
{
    Image *image;
    ExceptionInfo exception;
    unsigned int r;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    r = IsMonochromeImage(image, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return r ? Qtrue : Qfalse;
}

/*
    Method:     Image#montage, montage=
    Purpose:    Tile size and offset within an image montage.
                Only valid for montage images.
*/
DEF_ATTR_READER(Image, montage, str)

VALUE
Image_montage_eq(
    VALUE self,
    VALUE montage)
{
    Image *image;

    rb_warning("montage= is deprecated. It has no purpose.");
    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    if (montage == Qnil)
    {
        magick_free(image->montage);
        image->montage = NULL;
        return self;
    }
    magick_clone_string(&image->montage, STRING_PTR(montage));
    return self;
}


/*
    Static:     motion_blur(int argc, VALUE *argv, VALUE self, Image *fp)
    Purpose:    called from Image_motion_blur and Image_sketch
*/
static VALUE
motion_blur(
    int argc,
    VALUE *argv,
    VALUE self,
    Image *fp(const Image *, const double, const double, const double, ExceptionInfo *))
{
    Image *image, *new_image;
    double radius = 0.0;
    double sigma = 1.0;
    double angle = 0.0;
    ExceptionInfo exception;

    switch (argc)
    {
        case 3:
            angle = NUM2DBL(argv[2]);
        case 2:
            sigma = NUM2DBL(argv[1]);
        case 1:
            radius = NUM2DBL(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 to 3)", argc);
            break;
    }

    if (sigma == 0.0)
    {
        rb_raise(rb_eArgError, "sigma must be != 0.0");
    }

    Data_Get_Struct(self, Image, image);

    GetExceptionInfo(&exception);
    new_image = (fp)(image, radius, sigma, angle, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}


/*
    Method:     Image#motion_blur(radius=0.0, sigma=1.0, angle=0.0)
    Purpose:    simulates motion blur. Convolves the image with a Gaussian
                operator of the given radius and standard deviation (sigma).
                For reasonable results, radius should be larger than sigma.
                Use a radius of 0 and motion_blur selects a suitable radius
                for you. Angle gives the angle of the blurring motion.
*/
VALUE
Image_motion_blur(int argc, VALUE *argv, VALUE self)
{
    return motion_blur(argc, argv, self, MotionBlurImage);
}


/*
    Method:     Image#negate(grayscale=false)
    Purpose:    negates the colors in the reference image. The grayscale option
                means that only grayscale values within the image are negated.
    Notes:      The default for grayscale is false.

*/
VALUE
Image_negate(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    unsigned int grayscale = False;

    if (argc == 1)
    {
        grayscale = RTEST(argv[0]);
    }
    else if (argc > 1)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 or 1)", argc);
    }

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    (void) NegateImage(new_image, grayscale);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}



/*
 *  Method:     Image#negate_channel(grayscale=false, channel=AllChannels)
 *  Returns     a new image
*/
VALUE
Image_negate_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_NEGATEIMAGECHANNEL)
    Image *image, *new_image;
    ChannelType channels;
    unsigned int grayscale = False;

    channels = extract_channels(&argc, argv);

    // There can be at most 1 remaining argument.
    if (argc > 1)
    {
        raise_ChannelType_error(argv[argc-1]);
    }
    else if (argc == 1)
    {
        grayscale = RTEST(argv[0]);
    }

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    (void)NegateImageChannel(new_image, channels, grayscale);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image.new(cols, rows<, fill>) <{info block}>
    Purpose:    Create a new Image with "cols" columns and "rows" rows.
                If the fill argument is omitted, fill with the background color
    Returns:    A new Image
    Note:       This routine creates an Info structure to use when allocating
                the Image structure. The caller can supply an info parm block to
                use for initializing the Info.
*/
#if !defined(HAVE_RB_DEFINE_ALLOC_FUNC)
VALUE
Image_new(int argc, VALUE *argv, VALUE class)
{
    Info *info;
    Image *image;
    volatile VALUE info_obj;
    volatile VALUE new_image;
    VALUE init_arg[4];

    if (argc < 2 || argc > 3)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 or 3)", argc);
    }

    // Create a new Info object to use when creating this image.
    info_obj = rm_info_new();
    Data_Get_Struct(info_obj, Info, info);

    image = AllocateImage(info);
    new_image = Data_Wrap_Struct(class, NULL, DestroyImage, image);
    init_arg[0] = info_obj;
    init_arg[1] = argv[0];
    init_arg[2] = argv[1];
    init_arg[3] = argc == 3 ? argv[2] : 0;

    rb_obj_call_init((VALUE)new_image, 4, init_arg);
    return new_image;
}

/*
    Method:     Image#initialize
    Purpose:    initialize a new Image object
                If the fill argument is omitted, fill with the background color
*/
VALUE
Image_initialize(VALUE self, VALUE info_obj, VALUE width, VALUE height, VALUE fill)
{
    Image *image;
    Info *info;
    unsigned long cols, rows;

    cols = NUM2ULONG(width);
    rows = NUM2ULONG(height);

    if (cols <= 0 || rows <= 0)
    {
        rb_raise(rb_eArgError, "invalid image size (%dx%d)", cols, rows);
    }

    Data_Get_Struct(info_obj, Info, info);
    Data_Get_Struct(self, Image, image);

    SetImageExtent(image, cols, rows);

    // If the caller did not supply a fill argument, call SetImage to fill the
    // image using the background color. The background color can be set by
    // specifying it when creating the Info parm block.
    if (!fill)
    {
        SetImageBackgroundColor(image);
    }
    // fillobj.fill(self)
    else
    {
        (void) rb_funcall(fill, rm_ID_fill, 1, self);
    }

    return self;
}
#else


/*
    Extern:     Image_alloc(cols,rows,[fill])
    Purpose:    "allocate" a new Image object
    Note:       actually we defer allocating the image
                until the initialize method so we can
                run the parm block if it's present
*/
VALUE
Image_alloc(VALUE class)
{
    volatile VALUE image_obj;

    image_obj = Data_Wrap_Struct(class, NULL, DestroyImage, NULL);
    return image_obj;
}

/*
    Method:     Image#initialize(cols,rows,[fill])
    Purpose:    initialize a new Image object
                If the fill argument is omitted, fill with background color
*/
VALUE
Image_initialize(int argc, VALUE *argv, VALUE self)
{
    volatile VALUE fill = 0;
    Info *info;
    volatile VALUE info_obj;
    Image *image;
    unsigned long cols, rows;

    switch (argc)
    {
        case 3:
            fill = argv[2];
        case 2:
            rows = NUM2ULONG(argv[1]);
            cols = NUM2ULONG(argv[0]);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 or 3)", argc);
            break;
    }

    // Create a new Info object to use when creating this image.
    info_obj = rm_info_new();
    Data_Get_Struct(info_obj, Info, info);

    image = AllocateImage(info);
    if (!image)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }

    // NOW store a real image in the image object.
    DATA_PTR(self) = image;

    SetImageExtent(image, cols, rows);

    // If the caller did not supply a fill argument, call SetImage to fill the
    // image using the background color. The background color can be set by
    // specifying it when creating the Info parm block.
    if (!fill)
    {
        (void) SetImageBackgroundColor(image);
    }
    // fillobj.fill(self)
    else
    {
        (void) rb_funcall(fill, rm_ID_fill, 1, self);
    }

    return self;
}
#endif


/*
    External:   rm_image_new(Image *)
    Purpose:    create a new Image object from an Image structure
    Notes:      since the Image is already created we don't need
                to call Image_alloc or Image_initialize.
*/
VALUE
rm_image_new(Image *image)
{
    if (!image)
    {
        rb_bug("rm_image_new called with NULL argument");
    }
    return Data_Wrap_Struct(Class_Image, NULL, DestroyImage, image);
}

/*
    Method:     Image#normalize
    Purpose:    enhances the contrast of a color image by adjusting the pixels
                color to span the entire range of colors available
*/
VALUE
Image_normalize(VALUE self)
{
    Image *image, *new_image;

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    (void) NormalizeImage(new_image);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}


/*
    Method:     Image#normalize_channel(channel=AllChannels)
    Purpose:    Call NormalizeImageChannel
*/
VALUE
Image_normalize_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_NORMALIZEIMAGECHANNEL)
    Image *image, *new_image;
    ChannelType channels;

    channels = extract_channels(&argc, argv);
    // Ensure all arguments consumed.
    if (argc > 0)
    {
        raise_ChannelType_error(argv[argc-1]);
    }

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    (void) NormalizeImageChannel(new_image, channels);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

DEF_ATTR_READERF(Image, normalized_mean_error, error.normalized_mean_error, dbl)
DEF_ATTR_READERF(Image, normalized_maximum_error, error.normalized_maximum_error, dbl)

/*
    Method:     Image#number_colors
    Purpose:    return the number of unique colors in the image
*/
VALUE
Image_number_colors(VALUE self)
{
    Image *image;
    ExceptionInfo exception;
    unsigned long n = 0;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    n = (unsigned long) GetNumberColors(image, NULL, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return ULONG2NUM(n);
}

DEF_ATTR_ACCESSOR(Image, offset, long)

/*
    Method:     Image#oil_paint(radius=3.0)
    Purpose:    applies a special effect filter that simulates an oil painting
*/
VALUE
Image_oil_paint(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double radius = 3.0;
    ExceptionInfo exception;

    switch (argc)
    {
        case 1:
            radius = NUM2DBL(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 or 1)", argc);
            break;
    }
    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = OilPaintImage(image, radius, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Method:     Image#opaque(target-color-name, fill-color-name)
                Image#opaque(target-pixel, fill-pixel)
    Purpose:    changes any pixel that matches target with the color defined by fill
    Notes:      By default a pixel must match the specified target color exactly.
                Use image.fuzz to set the amount of tolerance acceptable to consider
                two colors as the same.
*/
VALUE
Image_opaque(VALUE self, VALUE target, VALUE fill)
{
    Image *image, *new_image;
    PixelPacket target_pp;
    PixelPacket fill_pp;

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    // Allow color name or Pixel
    Color_to_PixelPacket(&target_pp, target);
    Color_to_PixelPacket(&fill_pp, fill);

    (void) OpaqueImage(new_image, target_pp, fill_pp);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}

/*
    Method:     Image#opaque?
    Purpose:    return true if any of the pixels in the image have an opacity
                value other than opaque ( 0 )
*/
VALUE
Image_opaque_q(VALUE self)
{
    Image *image;
    ExceptionInfo exception;
    unsigned int r = 0;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    r = IsOpaqueImage(image, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return r ? Qtrue : Qfalse;
}

/*
    Method:     Image#ordered_dither(threshold_map='2x2')
    Purpose:    perform ordered dither on image
    Notes:      order must be 2, 3, or 4
                (6.3.0) order can be any of the threshold strings listed
                by "convert -list Thresholds" but the default is still "2x2".
                I don't call OrderedDitherImages anymore. Sometime after
                IM 6.0.0 it quit working. IM and GM use the routines I use
                below to implement the "ordered-dither" option.
*/
VALUE
Image_ordered_dither(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    int order;
    const char *threshold_map = "2x2";
    ExceptionInfo exception;

    if (argc > 1)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 or 1)", argc);
    }
    if (argc == 1)
    {
        if (TYPE(argv[0]) == T_STRING)
        {
            threshold_map = STRING_PTR(argv[0]);
        }
        else
        {
            order = NUM2INT(argv[0]);
            if (order == 3)
            {
                threshold_map = "3x3";
            }
            else if (order == 4)
            {
                threshold_map = "4x4";
            }
            else if (order != 2)
            {
                rb_raise(rb_eArgError, "order must be 2, 3, or 4 (%d given)", order);
            }
        }
    }


    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    GetExceptionInfo(&exception);

#if defined(HAVE_ORDEREDPOSTERIZEIMAGECHANNEL)
    // ImageMagick >= 6.3.0
    (void) OrderedPosterizeImage(new_image, threshold_map, &exception);
#elif defined(HAVE_RANDOMTHRESHOLDIMAGECHANNEL)
    // ImageMagick 6.0.0 thru 6.3.0
    (void) RandomThresholdImageChannel(new_image, AllChannels, threshold_map, &exception);
#else
    // GraphicsMagick
    (void) RandomChannelThresholdImage(new_image, "all", threshold_map, &exception);
#endif
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    return rm_image_new(new_image);
}


/*
    Method:     Image#orientation
    Purpose:    Return the orientation attribute as an OrientationType enum value.
*/
VALUE
Image_orientation(VALUE self)
{
#if defined(HAVE_IMAGE_ORIENTATION)
    Image *image;

    Data_Get_Struct(self, Image, image);
    return OrientationType_new(image->orientation);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#orientation=
    Purpose:    Set the orientation attribute
*/
VALUE
Image_orientation_eq(VALUE self, VALUE orientation)
{
#if defined(HAVE_IMAGE_ORIENTATION)
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    VALUE_TO_ENUM(orientation, image->orientation, OrientationType);
    return self;

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#page
    Purpose:    the page attribute getter
*/
VALUE
Image_page(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return Rectangle_from_RectangleInfo(&image->page);
}


/*
    Method:     Image#page=
    Purpose:    the page attribute setter
*/
VALUE
Image_page_eq(VALUE self, VALUE rect)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    Rectangle_to_RectangleInfo(&image->page, rect);
    return self;
}

/*
    Method:     Image#palette?
    Purpose:    return true if the image is PseudoClass and has 256 unique
                colors or less.
*/
VALUE
Image_palette_q(VALUE self)
{
    Image *image;
    ExceptionInfo exception;
    unsigned int r = 0;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    r = IsPaletteImage(image, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return r ? Qtrue : Qfalse;
}

/*
    Method:     Image.ping(file)
    Purpose:    Call ImagePing
    Returns:    Same as Image.read, except that PingImage does not
                return the pixel data.
*/
VALUE
Image_ping(VALUE class, VALUE file_arg)
{
    return rd_image(class, file_arg, PingImage);
}

/*
    Method:     Image#pixel_color(x, y<, color>)
    Purpose:    Gets/sets the color of the pixel at x,y
    Returns:    Magick::Pixel for pixel x,y. If called to set a new
                color, the return value is the old color.
    Notes:      "color", if present, may be either a color name or a
                Magick::Pixel.
                Based on Magick++'s Magick::pixelColor methods
*/
VALUE
Image_pixel_color(
    int argc,
    VALUE *argv,
    VALUE self)
{
    Image *image;
    PixelPacket old_color = {0}, new_color, *pixel;
    ExceptionInfo exception;
    long x, y;
    unsigned int set = False;
    unsigned int okay;

    switch (argc)
    {
        case 3:
            rm_check_frozen(self);
            set = True;
            // Replace with new color? The arg can be either a color name or
            // a Magick::Pixel.
            Color_to_PixelPacket(&new_color, argv[2]);
        case 2:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 or 3)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);
    x = NUM2LONG(argv[0]);
    y = NUM2LONG(argv[1]);

    // Get the color of a pixel
    if (!set)
    {
        GetExceptionInfo(&exception);
        old_color = *AcquireImagePixels(image, x, y, 1, 1, &exception);
        CHECK_EXCEPTION()

        (void) DestroyExceptionInfo(&exception);

        // PseudoClass
        if (image->storage_class == PseudoClass)
        {
            IndexPacket *indexes = GetIndexes(image);
            old_color = image->colormap[*indexes];
        }
        if (!image->matte)
        {
            old_color.opacity = OpaqueOpacity;
        }
        return Pixel_from_PixelPacket(&old_color);
    }

    // ImageMagick segfaults if the pixel location is out of bounds.
    // Do what IM does and return the background color.
    if (x < 0 || y < 0 || (unsigned long)x >= image->columns || (unsigned long)y >= image->rows)
    {
        return Pixel_from_PixelPacket(&image->background_color);
    }

    // Set the color of a pixel. Return previous color.
    // Convert to DirectClass
    if (image->storage_class == PseudoClass)
    {
        (void) SyncImage(image);
        magick_free(image->colormap);
        image->colormap = NULL;
        image->storage_class = DirectClass;
    }

    pixel = GetImagePixels(image, x, y, 1, 1);
    if (pixel)
    {
        old_color = *pixel;
        if (!image->matte)
        {
            old_color.opacity = OpaqueOpacity;
        }
    }
    *pixel = new_color;
    okay = SyncImagePixels(image);
    if (!okay)
    {
        rb_raise(Class_ImageMagickError, "image pixels could not be synced");
    }

    return Pixel_from_PixelPacket(&old_color);
}


/*
    Method:     Image.pixel_interpolation_method
                Image.pixel_interpolation_method=method
    Purpose:    Get/set the "interpolate" field in the Image structure.
    Ref:        Image.interpolate_pixel_color
*/
VALUE
Image_pixel_interpolation_method(VALUE self)
{
#if defined(HAVE_INTERPOLATEPIXELCOLOR)
    Image *image;

    Data_Get_Struct(self, Image, image);
    return InterpolatePixelMethod_new(image->interpolate);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


VALUE
Image_pixel_interpolation_method_eq(VALUE self, VALUE method)
{
#if defined(HAVE_INTERPOLATEPIXELCOLOR)
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    VALUE_TO_ENUM(method, image->interpolate, InterpolatePixelMethod);
    return self;

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


#if 0
/*
    Method:     Image.plasma(x1, y1, x2, y2, attenuate, depth)
                x1, y1, x2, y2 - the region to apply plasma fractals values
                attenuate - the plasma attenuation factor
                depth - the plasma recursion depth
    Purpose:    initializes an image with plasma fractal values. The image must
                be initialized with a base color before this method is called.
    Returns:    A new Image
*/
VALUE
Image_plasma(
    VALUE self,
    VALUE x1,
    VALUE y1,
    VALUE x2,
    VALUE y2,
    VALUE attenuate,
    VALUE depth)
{
    Image *image, *new_image;
    SegmentInfo segment;
    unsigned int okay;

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    segment.x1 = NUM2DBL(x1);
    segment.y1 = NUM2DBL(y1);
    segment.x2 = NUM2DBL(x2);
    segment.y2 = NUM2DBL(y2);
    srand((unsigned int) time(NULL));
    okay = PlasmaImage(new_image, &segment, NUM2INT(attenuate), NUM2INT(depth));
    if (!okay)
    {
        rb_warning("RMagick: invalid region - plasma failed.");
    }
    return rm_image_new(new_image);
}
#endif


/*
    Method:     Image#polaroid([angle=-5])
    Purpose:    Call PolaroidImage
    Notes:      Accepts an options block to get Draw attributes for drawing
                the label. Specify self.border_color to set a non-default
                border color.
*/
VALUE
Image_polaroid(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_POLAROIDIMAGE)
    Image *image, *clone, *new_image;
    volatile VALUE options;
    double angle = -5.0;
    Draw *draw;
    ExceptionInfo exception;

    GetExceptionInfo(&exception);

    Data_Get_Struct(self, Image, image);

    switch (argc)
    {
        case 1:
            angle = NUM2DBL(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 or 1)", argc);
            break;
    }

    options = rm_polaroid_new();
    Data_Get_Struct(options, Draw, draw);

    clone = CloneImage(image, 0, 0, MagickTrue, &exception);
    rm_check_exception(&exception, clone, DestroyOnError);

    clone->background_color = draw->shadow_color;

    new_image = PolaroidImage(clone, draw->info, angle, &exception);

    rm_check_exception(&exception, clone, DestroyOnError);
    (void) DestroyImage(clone);
    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}




/*
    Method:     posterize
    Purpose:    call PosterizeImage
    Notes:      Image#posterize(levels=4, dither=false)
*/
VALUE
Image_posterize(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_POSTERIZEIMAGE)
    Image *image, *new_image;
    MagickBooleanType dither = MagickFalse;
    unsigned long levels = 4;

    switch(argc)
    {
        case 2:
            dither = (MagickBooleanType) RTEST(argv[1]);
            /* fall through */
        case 1:
            levels = NUM2ULONG(argv[0]);
            /* fall through */
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 2)", argc);
    }

    Data_Get_Struct(self, Image, image);
    new_image = rm_clone_image(image);

    (void) PosterizeImage(new_image, levels, dither);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:  preview
    Purpose: Call PreviewImage
*/
VALUE
Image_preview(VALUE self, VALUE preview)
{
#if defined(HAVE_PREVIEWIMAGE)
    Image *image, *new_image;
    PreviewType preview_type;
    ExceptionInfo exception;

    GetExceptionInfo(&exception);

    VALUE_TO_ENUM(preview, preview_type, PreviewType);

    Data_Get_Struct(self, Image, image);
    new_image = PreviewImage(image, preview_type, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#profile!(name, profile)
    Purpose:    If "profile" is nil, deletes the profile. Otherwise "profile"
                must be a string containing the specified profile.
*/
VALUE
Image_profile_bang(VALUE self, VALUE name, VALUE profile)
{

    if (profile == Qnil)
    {
        return Image_delete_profile(self, name);
    }
    else
    {
        return set_profile(self, STRING_PTR(name), profile);
    }

}



#if defined(HAVE_IMAGE_QUALITY)
DEF_ATTR_READER(Image, quality, ulong)
#endif



/*
    Method:     Image#quantum_depth -> 8, 16, or 32
    Purpose:    Return image depth to nearest quantum
    Notes:      IM 6.0.0 introduced GetImageQuantumDepth, IM 6.0.5
                added a 2nd argument. The MagickFalse argument
                gives the 6.0.5 version the same behavior as before.
*/
VALUE
Image_quantum_depth(VALUE self)
{
#if defined(HAVE_GETIMAGEQUANTUMDEPTH)
    Image *image;
    unsigned long quantum_depth;

    Data_Get_Struct(self, Image, image);
#if defined(HAVE_OLD_GETIMAGEQUANTUMDEPTH)
    quantum_depth = GetImageQuantumDepth(image);
#else
    quantum_depth = GetImageQuantumDepth(image, MagickFalse);
#endif

    rm_check_image_exception(image, RetainOnError);

    return ULONG2NUM(quantum_depth);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#quantum_operator(operator, rvalue[, channel] )
    Purpose:    This method is an adapter method that calls either the
                    QuantumOperatorRegionImage method (GraphicsMagick 1.1) or the
                    EvaluateImageChannel method (ImageMagick 6.0.0)
    Note 1:     By necessity this method implements the "lowest common denominator"
                of the two implementations.

    Note 2:     If the channel argument is omitted, the default is AllChannels.
*/
VALUE
Image_quantum_operator(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_QUANTUMOPERATORREGIONIMAGE)
    Image *image;
    QuantumExpressionOperator operator;
    QuantumOperator qop;
    ChannelType channel;
    double rvalue;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    // The default channel is AllChannels
    channel = AllChannels;

    /*
        If there are 3 arguments, argument 2 is a ChannelType argument.
        Arguments 1 and 0 are required and are the rvalue and operator,
        respectively.
    */
    switch(argc)
    {
        case 3:
            VALUE_TO_ENUM(argv[2], channel, ChannelType);
            /* Fall through */
        case 2:
            rvalue = NUM2DBL(argv[1]);
            VALUE_TO_ENUM(argv[0], operator, QuantumExpressionOperator);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 or 3)", argc);
            break;
    }

    // Map QuantumExpressionOperator to QuantumOperator
    switch(operator)
    {
        default:
        case UndefinedQuantumOperator:
            qop = UndefinedQuantumOp;
            break;
        case AddQuantumOperator:
            qop = AddQuantumOp;
            break;
        case AndQuantumOperator:
            qop = AndQuantumOp;
            break;
        case DivideQuantumOperator:
            qop = DivideQuantumOp;
            break;
        case LShiftQuantumOperator:
            qop = LShiftQuantumOp;
            break;
        case MultiplyQuantumOperator:
            qop = MultiplyQuantumOp;
            break;
        case OrQuantumOperator:
            qop = OrQuantumOp;
            break;
        case RShiftQuantumOperator:
            qop = RShiftQuantumOp;
            break;
        case SubtractQuantumOperator:
            qop = SubtractQuantumOp;
            break;
        case XorQuantumOperator:
            qop = XorQuantumOp;
            break;
    }

    GetExceptionInfo(&exception);
    (void) QuantumOperatorImage(image, channel, qop, rvalue, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return self;

#elif defined(HAVE_EVALUATEIMAGECHANNEL)
    Image *image;
    QuantumExpressionOperator operator;
    MagickEvaluateOperator qop;
    double rvalue;
    ChannelType channel;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    // The default channel is AllChannels
    channel = AllChannels;

    /*
        If there are 3 arguments, argument 2 is a ChannelType argument.
        Arguments 1 and 0 are required and are the rvalue and operator,
        respectively.
    */
    switch(argc)
    {
        case 3:
            VALUE_TO_ENUM(argv[2], channel, ChannelType);
            /* Fall through */
        case 2:
            rvalue = NUM2DBL(argv[1]);
            VALUE_TO_ENUM(argv[0], operator, QuantumExpressionOperator);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 or 3)", argc);
            break;
    }

    // Map QuantumExpressionOperator to MagickEvaluateOperator
    switch(operator)
    {
        default:
        case UndefinedQuantumOperator:
            qop = UndefinedEvaluateOperator;
            break;
        case AddQuantumOperator:
            qop = AddEvaluateOperator;
            break;
        case AndQuantumOperator:
            qop = AndEvaluateOperator;
            break;
        case DivideQuantumOperator:
            qop = DivideEvaluateOperator;
            break;
        case LShiftQuantumOperator:
            qop = LeftShiftEvaluateOperator;
            break;
#if defined(HAVE_MAXEVALUATEOPERATOR)
        case MaxQuantumOperator:
            qop = MaxEvaluateOperator;
            break;
        case MinQuantumOperator:
            qop = MinEvaluateOperator;
            break;
#endif
        case MultiplyQuantumOperator:
            qop = MultiplyEvaluateOperator;
            break;
        case OrQuantumOperator:
            qop = OrEvaluateOperator;
            break;
        case RShiftQuantumOperator:
            qop = RightShiftEvaluateOperator;
            break;
        case SubtractQuantumOperator:
            qop = SubtractEvaluateOperator;
            break;
        case XorQuantumOperator:
            qop = XorEvaluateOperator;
            break;
    }

    GetExceptionInfo(&exception);
    (void) EvaluateImageChannel(image, channel, qop, rvalue, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return self;

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#quantize(<number_colors<, colorspace<, dither<, tree_depth<, measure_error>>>>>)
                defaults: 256, Magick::RGBColorspace, true, 0, false
    Purpose:    call QuantizeImage
*/
VALUE
Image_quantize(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    QuantizeInfo quantize_info;

    Data_Get_Struct(self, Image, image);
    GetQuantizeInfo(&quantize_info);

    switch (argc)
    {
        case 5:
            quantize_info.measure_error = (MagickBooleanType) RTEST(argv[4]);
        case 4:
            quantize_info.tree_depth = NUM2UINT(argv[3]);
        case 3:
            quantize_info.dither = (MagickBooleanType) RTEST(argv[2]);
        case 2:
            VALUE_TO_ENUM(argv[1], quantize_info.colorspace, ColorspaceType);
        case 1:
            quantize_info.number_colors = NUM2UINT(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 5)", argc);
            break;
    }

    new_image = rm_clone_image(image);

    (void) QuantizeImage(&quantize_info, new_image);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}



/*
    Method:     Image#radial_blur(angle)
    Purpose:    Call RadialBlurImage
    Notes:      Angle is in degrees
*/
VALUE
Image_radial_blur(VALUE self, VALUE angle)
{
#if defined(HAVE_RADIALBLURIMAGE)
    Image *image, *new_image;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = RadialBlurImage(image, NUM2DBL(angle), &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:     Image#radial_blur_channel(angle[, channel..])
    Purpose:    Call RadialBlurImageChannel
    Notes:      Angle is in degrees
*/
VALUE
Image_radial_blur_channel(
    int argc,
    VALUE *argv,
    VALUE self)
{
#if defined(HAVE_RADIALBLURIMAGECHANNEL)
    Image *image, *new_image;
    ExceptionInfo exception;
    ChannelType channels;

    channels = extract_channels(&argc, argv);

    // There must be 1 remaining argument.
    if (argc == 0)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (0 for 1 or more)");
    }
    else if (argc > 1)
    {
        raise_ChannelType_error(argv[argc-1]);
    }

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = RadialBlurImageChannel(image, channels, NUM2DBL(argv[0]), &exception);

    rm_check_exception(&exception, new_image, DestroyOnError);
    (void) DestroyExceptionInfo(&exception);
    rm_ensure_result(new_image);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#random_channel_threshold
    Purpose:    changes the value of individual pixels based on the intensity of
                each pixel compared to a random threshold. The result is a
                low-contrast, two color image.
    Args:       `channel_arg' can be "all", "intensity", "opacity", "matte"
                `thresholds' can be '2x2', '3x3', '4x4', a single digit,
                (treated as 'X', and Y=MaxRGB-X), 'XxY'. If the thresholds
                string includes a '%', the number(s) is/are treated as percentage(s)
                of MaxRGB.
    Notes:      Christy says TO DO: red, green, blue, cyan, magenta, yellow, black.
                Added in 5.5.7, deprecated in 6.0.0.
*/
VALUE
Image_random_channel_threshold(
    VALUE self,
    VALUE channel_arg,
    VALUE thresholds_arg)
{
#if defined(HAVE_RANDOMCHANNELTHRESHOLDIMAGE)
    Image *image, *new_image;
    char *channel, *thresholds;
    ExceptionInfo exception;

#if defined(HAVE_RANDOMTHRESHOLDIMAGECHANNEL)
    rb_warning("This method is deprecated in this release of " Q(MAGICKNAME)
               ". Use Image#random_threshold_channel instead.");
#endif

    Data_Get_Struct(self, Image, image);

    channel = STRING_PTR(channel_arg);
    thresholds = STRING_PTR(thresholds_arg);

    new_image = rm_clone_image(image);

    GetExceptionInfo(&exception);
    (void) RandomChannelThresholdImage(new_image, channel, thresholds, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#random_threshold_channel
    PUrpose:    Call RandomThresholdImageChannel
    Notes:      Very similar to above. This method was added in IM 6.0.0
                and the RandomChannelThresholdImage method was deprecated.
*/
VALUE
Image_random_threshold_channel(
    int argc,
    VALUE *argv,
    VALUE self)
{
#if defined(HAVE_RANDOMTHRESHOLDIMAGECHANNEL)
    Image *image, *new_image;
    ChannelType channels;
    char *thresholds;
    volatile VALUE geom_str;
    ExceptionInfo exception;


    Data_Get_Struct(self, Image, image);

    channels = extract_channels(&argc, argv);

    // There must be 1 remaining argument.
    if (argc == 0)
    {
        rb_raise(rb_eArgError, "missing threshold argument");
    }
    else if (argc > 1)
    {
        raise_ChannelType_error(argv[argc-1]);
    }

    // Accept any argument that has a to_s method.
    geom_str = rb_funcall(argv[0], rm_ID_to_s, 0);
    thresholds = STRING_PTR(geom_str);

    new_image = rm_clone_image(image);

    GetExceptionInfo(&exception);

    (void) RandomThresholdImageChannel(new_image, channels, thresholds, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#raise(width=6, height=6, raised=true)
    Purpose:    creates a simulated three-dimensional button-like effect by
                lightening and darkening the edges of the image. The "width"
                and "height" arguments define the width of the vertical and
                horizontal edge of the effect. If "raised" is true, creates
                a raised effect, otherwise a lowered effect.
    Returns:    a new image
*/
VALUE
Image_raise(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    RectangleInfo rect = {0};
    int raised = True;      // default

    rect.width = 6;         // default
    rect.height = 6;        // default

    switch (argc)
    {
        case 3:
            raised = RTEST(argv[2]);
        case 2:
            rect.height = NUM2ULONG(argv[1]);
        case 1:
            rect.width = NUM2ULONG(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 3)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    (void) RaiseImage(new_image, &rect, raised);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}

/*
    Method:     Image.read(file)
    Purpose:    Call ReadImage
    Returns:    An array of 1 or more new image objects.
*/
VALUE
Image_read(VALUE class, VALUE file_arg)
{
    return rd_image(class, file_arg, ReadImage);
}


/*
 *  Static:     file_arg_rescue
 *  Purpose:    called when `rm_obj_to_s' raised an exception
*/
static VALUE
file_arg_rescue(VALUE arg)
{
    rb_raise(rb_eTypeError, "argument must be path name or open file (%s given)",
            rb_class2name(CLASS_OF(arg)));
}


/*
    Static:     rd_image(class, file, reader)
    Purpose:    Transform arguments, call either ReadImage or PingImage
    Returns:    see Image_read or Image_ping
    Notes:      yields to a block to get Image::Info attributes
                before calling Read/PingImage
*/
static VALUE
rd_image(VALUE class, VALUE file, reader_t reader)
{
    char *filename;
    long filename_l;
    Info *info;
    volatile VALUE info_obj;
    Image *images;
    ExceptionInfo exception;

    // Create a new Info structure for this read/ping
    info_obj = rm_info_new();
    Data_Get_Struct(info_obj, Info, info);

    if (TYPE(file) == T_FILE)
    {
        OpenFile *fptr;

        // Ensure file is open - raise error if not
        GetOpenFile(file, fptr);
        rb_io_check_readable(fptr);
        SetImageInfoFile(info, GetReadFile(fptr));
    }
    else
    {
        // Convert arg to string. If an exception occurs raise an error condition.
        file = rb_rescue(rb_String, file, file_arg_rescue, file);

        filename = STRING_PTR_LEN(file, filename_l);
        filename_l = min(filename_l, MaxTextExtent-1);
        memcpy(info->filename, filename, (size_t)filename_l);
        info->filename[filename_l] = '\0';
        SetImageInfoFile(info, NULL);
    }

    GetExceptionInfo(&exception);

    images = (reader)(info, &exception);
    rm_check_exception(&exception, images, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    return array_from_images(images);
}


/*
    Method:     Image#recolor(matrix)
    Purpose:    Call RecolorImage
*/
VALUE
Image_recolor(VALUE self, VALUE color_matrix)
{
#if defined(HAVE_RECOLORIMAGE)
    Image *image, *new_image;
    unsigned long order;
    long x, len;
    double *matrix;
    ExceptionInfo exception;

    GetExceptionInfo(&exception);

    // Allocate color matrix from Ruby's memory
    len = RARRAY(color_matrix)->len;
    matrix = ALLOC_N(double, len);

    for (x = 0; x < len; x++)
    {
        matrix[x] = NUM2DBL(rb_ary_entry(color_matrix, x));
    }

    order = (unsigned long)sqrt((double)(len + 1.0));

    Data_Get_Struct(self, Image, image);

    // RecolorImage sets the ExceptionInfo and returns a NULL image if an error occurs.
    new_image = RecolorImage(image, order, matrix, &exception);
    xfree((void *)matrix);

    rm_check_exception(&exception, new_image, DestroyOnError);
    (void) DestroyExceptionInfo(&exception);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image.read_inline(content)
    Purpose:    Read a Base64-encoded image
    Returns:    an array of new images
    Notes:      this is similar to, but not the same as ReadInlineImage.
                ReadInlineImage requires a comma preceeding the image
                data. This method allows but does not require a comma.
*/
VALUE
Image_read_inline(VALUE self, VALUE content)
{
     volatile VALUE info_obj;
     Image *images;
     ImageInfo *info;
     char *image_data;
     long x, image_data_l;
     unsigned char *blob;
     size_t blob_l;
     ExceptionInfo exception;

     image_data = STRING_PTR_LEN(content, image_data_l);

     // Search for a comma. If found, we'll set the start of the
     // image data just following the comma. Otherwise we'll assume
     // the image data starts with the first byte.
     for(x = 0; x < image_data_l; x++)
     {
          if (image_data[x] == ',')
          {
               break;
          }
     }
     if (x < image_data_l)
     {
          image_data += x + 1;
     }

     blob = Base64Decode(image_data, &blob_l);
     if (blob_l == 0)
     {
          rb_raise(rb_eArgError, "can't decode image");
     }

     GetExceptionInfo(&exception);

     // Create a new Info structure for this read. About the
     // only useful attribute that can be set is `format'.
     info_obj = rm_info_new();
     Data_Get_Struct(info_obj, Info, info);

     images = BlobToImage(info, blob, blob_l, &exception);
     magick_free((void *)blob);

     rm_check_exception(&exception, images, DestroyOnError);

     (void) DestroyExceptionInfo(&exception);

     return array_from_images(images);
}


/*
     Static:    array_from_images
     Purpose:   convert a list of images to an array of Image objects
*/
static VALUE array_from_images(Image *images)
{
     volatile VALUE image_obj, image_ary;
     Image *image, *next;

     // Orphan the image, create an Image object, add it to the array.

     image_ary = rb_ary_new();
     next = NULL;
     next = next;        // defeat "never referenced" message from icc
     while (images)
     {
         image = RemoveFirstImageFromList(&images);
         image_obj = rm_image_new(image);
         (void) rb_ary_push(image_ary, image_obj);
     }

     return image_ary;
}


/*
    Method:     Image#reduce_noise(radius)
    Purpose:    smooths the contours of an image while still preserving edge information
    Returns:    a new image
*/
VALUE
Image_reduce_noise(VALUE self, VALUE radius)
{
    Image *image, *new_image;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = ReduceNoiseImage(image, NUM2DBL(radius), &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    return rm_image_new(new_image);
}

/*
    Method:     Image#rendering_intent=
    Purpose:    get rendering_intent
*/
VALUE
Image_rendering_intent(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return RenderingIntent_new(image->rendering_intent);
}

/*
    Method:     Image#rendering_intent=
    Purpose:    set rendering_intent
*/
VALUE
Image_rendering_intent_eq(VALUE self, VALUE ri)
{
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    VALUE_TO_ENUM(ri, image->rendering_intent, RenderingIntent);
    return self;
}

/*
    Method:     Image#resize(scale) or (cols, rows<, filter<, blur>>)
                Image#resize!(scale) or (cols, rows<, filter<, blur>>)
    Purpose:    scales an image to the desired dimensions using the specified filter
                and blur factor
    Returns:    resize: a resized copy of the input image
                resize!: self, resized
    Default:    if filter is not specified, use image->filter
                if blur is not specified, use image->blur
*/
static VALUE
resize(int bang, int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double scale;
    FilterTypes filter;
    unsigned long rows, columns;
    double blur, drows, dcols;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    // Set up defaults
    filter  = image->filter;
    blur    = image->blur;
    rows    = image->rows;
    columns = image->columns;

    switch (argc)
    {
        case 4:
            blur = NUM2DBL(argv[3]);
        case 3:
            VALUE_TO_ENUM(argv[2], filter, FilterTypes);
        case 2:
            rows = NUM2ULONG(argv[1]);
            columns = NUM2ULONG(argv[0]);
            if (columns == 0 || rows == 0)
            {
                rb_raise(rb_eArgError, "invalid result dimension (%lu, %lu given)", columns, rows);
            }
            break;
        case 1:
            scale = NUM2DBL(argv[0]);
            if (scale < 0.0)
            {
                rb_raise(rb_eArgError, "invalid scale value (%g given)", scale);
            }
            drows = scale * image->rows + 0.5;
            dcols = scale * image->columns + 0.5;
            if (drows > (double)ULONG_MAX || dcols > (double)ULONG_MAX)
            {
                rb_raise(rb_eRangeError, "resized image too big");
            }
            rows = (unsigned long) drows;
            columns = (unsigned long) dcols;
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 to 4)", argc);
            break;
    }

    GetExceptionInfo(&exception);
    new_image = ResizeImage(image, columns, rows, filter, blur, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    if (bang)
    {
        DATA_PTR(self) = new_image;
        (void) DestroyImage(image);
        return self;
    }
    return rm_image_new(new_image);
}

VALUE
Image_resize(int argc, VALUE *argv, VALUE self)
{
    return resize(False, argc, argv, self);
}

VALUE
Image_resize_bang(int argc, VALUE *argv, VALUE self)
{
    rm_check_frozen(self);
    return resize(True, argc, argv, self);
}

/*
    Method:     Image#roll(x_offset, y_offset)
    Purpose:    offsets an image as defined by x_offset and y_offset
    Returns:    a rolled copy of the input image
*/
VALUE
Image_roll(VALUE self, VALUE x_offset, VALUE y_offset)
{
    Image *image, *new_image;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = RollImage(image, NUM2LONG(x_offset), NUM2LONG(y_offset), &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}


/*
    Method:     Image#rotate(degrees [,'<' | '>'])
    Purpose:    creates a new image that is a rotated copy of an existing one
                Image#rotate!(degrees)
    Purpose:    rotates the image by the specified number of degrees
    Note:       If the 2nd argument is '<' rotate only if width < height.
                If the 2nd argument is '>' rotate only if width > height.
*/
static VALUE
rotate(int bang, int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double degrees;
    char *arrow;
    long arrow_l;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    switch (argc)
    {
        case 2:
            arrow = STRING_PTR_LEN(argv[1], arrow_l);
            if (arrow_l != 1 || (*arrow != '<' && *arrow != '>'))
            {
                rb_raise(rb_eArgError, "second argument must be '<' or '>', '%s' given", arrow);
            }
            if (*arrow == '>' && image->columns <= image->rows)
            {
                return Qnil;
            }
            if (*arrow == '<' && image->columns >= image->rows)
            {
                return Qnil;
            }
        case 1:
            degrees = NUM2DBL(argv[0]);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 or 2)", argc);
            break;
    }

    GetExceptionInfo(&exception);

    new_image = RotateImage(image, degrees, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    if (bang)
    {
        DATA_PTR(self) = new_image;
        (void) DestroyImage(image);
        return self;
    }
    return rm_image_new(new_image);
}

VALUE
Image_rotate(int argc, VALUE *argv, VALUE self)
{
    return rotate(False, argc, argv, self);
}

VALUE
Image_rotate_bang(int argc, VALUE *argv, VALUE self)
{
    rm_check_frozen(self);
    return rotate(True, argc, argv, self);
}

DEF_ATTR_READER(Image, rows, int)

/*
    Method:     Image#sample(scale) or (cols, rows)
                Image#sample!
    Purpose:    scales an image to the desired dimensions with pixel sampling
    Returns:    sampled: a sampled copy of the input image
                sample!: self, sampled
*/
VALUE
Image_sample(int argc, VALUE *argv, VALUE self)
{
    return scale(False, argc, argv, self, SampleImage);
}

VALUE
Image_sample_bang(int argc, VALUE *argv, VALUE self)
{
    rm_check_frozen(self);
    return scale(True, argc, argv, self, SampleImage);
}

/*
    Method:     Image#scale(scale) or (cols, rows)
                Image#scale!
    Purpose:    changes the size of an image to the given dimensions
    Returns:    scale: a scaled copy of the input image
                scale!: self, scaled
*/
VALUE
Image_scale(int argc, VALUE *argv, VALUE self)
{
    return scale(False, argc, argv, self, ScaleImage);
}

VALUE
Image_scale_bang(int argc, VALUE *argv, VALUE self)
{
    rm_check_frozen(self);
    return scale(True, argc, argv, self, ScaleImage);
}

/*
    Static:     scale
    Purpose:    Call ScaleImage or SampleImage
    Arguments:  if 1 argument > 0, multiply current size by this much
                if 2 arguments, (cols, rows)
*/
static VALUE
scale(int bang, int argc, VALUE *argv, VALUE self, scaler_t scaler)
{
    Image *image, *new_image;
    unsigned long columns, rows;
    double scale, drows, dcols;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    switch (argc)
    {
        case 2:
            columns = NUM2ULONG(argv[0]);
            rows    = NUM2ULONG(argv[1]);
            if (columns == 0 || rows == 0)
            {
                rb_raise(rb_eArgError, "invalid result dimension (%lu, %lu given)", columns, rows);
            }
            break;
        case 1:
            scale = NUM2DBL(argv[0]);
            if (scale <= 0)
            {
                rb_raise(rb_eArgError, "invalid scale value (%g given)", scale);
            }
            drows = scale * image->rows + 0.5;
            dcols = scale * image->columns + 0.5;
            if (drows > (double)ULONG_MAX || dcols > (double)ULONG_MAX)
            {
                rb_raise(rb_eRangeError, "resized image too big");
            }
            rows = (unsigned long) drows;
            columns = (unsigned long) dcols;
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 or 2)", argc);
            break;
    }

    GetExceptionInfo(&exception);
    new_image = (scaler)(image, columns, rows, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    if (bang)
    {
        DATA_PTR(self) = new_image;
        (void) DestroyImage(image);
        return self;
    }

    return rm_image_new(new_image);
}

DEF_ATTR_READER(Image, scene, ulong)


/*
 *  Method:     Image#set_channel_depth(channel, depth)
 *  Purpose:    Call SetImageChannelDepth
*/
VALUE
Image_set_channel_depth(VALUE self, VALUE channel_arg, VALUE depth)
{
#if defined(HAVE_SETIMAGECHANNELDEPTH)
     Image *image;
     ChannelType channel;
     unsigned long channel_depth;

     rm_check_frozen(self);
     Data_Get_Struct(self, Image, image);
     VALUE_TO_ENUM(channel_arg, channel, ChannelType);
     channel_depth = NUM2ULONG(depth);

     (void) SetImageChannelDepth(image, channel, channel_depth);
     rm_check_image_exception(image, RetainOnError);

     return self;
#else
     rm_not_implemented();
     return (VALUE)0;
#endif
}


/*
 * Method:  Image#sepiatone(threshold=MaxRGB)
 * Purpose: Call SepiaToneImage
*/
VALUE
Image_sepiatone(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_SEPIATONEIMAGE)
    Image *image, *new_image;
    double threshold = (double) MaxRGB;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    switch(argc)
    {
        case 1:
            threshold = NUM2DBL(argv[0]);
            break;
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 or 1)", argc);
    }

    new_image = SepiaToneImage(image, threshold, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#segment(colorspace=RGBColorspace,
                                   cluster_threshold=1.0,
                                   smoothing_threshold=1.5,
                                   verbose=false)
    Purpose:    Call SegmentImage
    Notes:      the default values are the same as Magick++
*/
VALUE
Image_segment(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    int colorspace              = RGBColorspace;    // These are the Magick++ defaults
    unsigned int verbose        = False;
    double cluster_threshold    = 1.0;
    double smoothing_threshold  = 1.5;

    switch (argc)
    {
        case 4:
            verbose = RTEST(argv[3]);
        case 3:
            smoothing_threshold = NUM2DBL(argv[2]);
        case 2:
            cluster_threshold = NUM2DBL(argv[1]);
        case 1:
            VALUE_TO_ENUM(argv[0], colorspace, ColorspaceType);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 4)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    (void) SegmentImage(new_image, colorspace, verbose, cluster_threshold, smoothing_threshold);
    rm_check_image_exception(new_image, DestroyOnError);
    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Method:     Image#opacity=
    Purpose:    Call SetImageOpacity
*/
VALUE
Image_opacity_eq(VALUE self, VALUE opacity_arg)
{
    Image *image;
    unsigned int opacity;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    opacity = NUM2UINT(opacity_arg);
    if (opacity > MaxRGB)
    {
        rb_raise(rb_eArgError, "opacity level (%d) exceeds MaxRGB", opacity);
    }

    (void) SetImageOpacity(image, (Quantum)opacity);
    return self;
}


/*
    Method:     Image#properties [{ |k,v| block }]
    Purpose:    Traverse the attributes and yield to the block.
                If no block, return a hash of all the attribute
                keys & values
    Notes:      I use the word "properties" to distinguish between
                these "user-added" attribute strings and Image
                object attributes.
*/
VALUE
Image_properties(VALUE self)
{
    Image *image;
    const ImageAttribute *attr;
    volatile VALUE attr_hash;

    Data_Get_Struct(self, Image, image);

    // If block, iterate over attributes
    if (rb_block_given_p())
    {
        volatile VALUE ary = rb_ary_new2(2);

#if defined(HAVE_GETNEXTIMAGEATTRIBUTE)
        ResetImageAttributeIterator(image);
        attr = GetNextImageAttribute(image);
        while (attr)
        {
            (void) rb_ary_store(ary, 0, rb_str_new2(attr->key));
            (void) rb_ary_store(ary, 1, rb_str_new2(attr->value));
            (void) rb_yield(ary);
            attr = GetNextImageAttribute(image);
        }
#else
        for (attr = image->attributes; attr; attr = Next_Attribute)
        {
            // Store the next ptr where Image#aset can see it.
            // The app may decide to delete that attribute.
            Next_Attribute = attr->next;
            rb_ary_store(ary, 0, rb_str_new2(attr->key));
            rb_ary_store(ary, 1, rb_str_new2(attr->value));
            rb_yield(ary);
        }
#endif
        return self;
    }

    // otherwise return properties hash
    else
    {
        attr_hash = rb_hash_new();
#if defined(HAVE_GETNEXTIMAGEATTRIBUTE)
        ResetImageAttributeIterator(image);
        attr = GetNextImageAttribute(image);
        while (attr)
        {
            (void) rb_hash_aset(attr_hash, rb_str_new2(attr->key), rb_str_new2(attr->value));
            attr = GetNextImageAttribute(image);
        }
#else
        for (attr = image->attributes; attr; attr = attr->next)
        {
            (void) rb_hash_aset(attr_hash, rb_str_new2(attr->key), rb_str_new2(attr->value));
        }
#endif
        return attr_hash;
    }
}


/*
    Method:     Image#shade(shading=false, azimuth=30, elevation=30)
    Purpose:    shines a distant light on an image to create a three-dimensional
                effect. You control the positioning of the light with azimuth
                and elevation; azimuth is measured in degrees off the x axis
                and elevation is measured in pixels above the Z axis
    Returns:    a new image
*/
VALUE
Image_shade(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double azimuth = 30.0, elevation = 30.0;
    unsigned int shading=False;
    ExceptionInfo exception;

    switch (argc)
    {
        case 3:
            elevation = NUM2DBL(argv[2]);
        case 2:
            azimuth = NUM2DBL(argv[1]);
        case 1:
            shading = RTEST(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 3)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = ShadeImage(image, shading, azimuth, elevation, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}


/*
    Method:     Image#shadow(x_offset=4, y_offset=4, sigma=4.0, opacity=1.0)
                x- and y-offsets are the pixel offset
                opacity is either a number between 0 and 1 or a string "NN%"
                sigma is the std. dev. of the Gaussian, in pixels.
    Purpose:    Call ShadowImage
    Notes:      The defaults are taken from the mogrify.c source, except
                for opacity, which has no default.
                Introduced in 6.1.7
*/
VALUE
Image_shadow(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_SHADOWIMAGE)
    Image *image, *new_image;
    double opacity = 100.0;
    double sigma = 4.0;
    long x_offset = 4L;
    long y_offset = 4L;
    ExceptionInfo exception;

    switch(argc)
    {
        case 4:
            opacity = rm_percentage(argv[3]);   // Clamp to 1.0 < x <= 100.0
            if (fabs(opacity) < 0.01)
            {
                rb_warning("shadow will be transparent - opacity %g very small", opacity);
            }
            opacity = FMIN(opacity, 1.0);
            opacity = FMAX(opacity, 0.01);
            opacity *= 100.0;
        case 3:
            sigma = NUM2DBL(argv[2]);
        case 2:
            y_offset = NUM2LONG(argv[1]);
        case 1:
            x_offset = NUM2LONG(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 4)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = ShadowImage(image, opacity, sigma, x_offset, y_offset, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:     Image#sharpen(radius=0, sigma=1)
    Purpose:    sharpens an image
    Returns:    a new image
*/
VALUE
Image_sharpen(int argc, VALUE *argv, VALUE self)
{
    return effect_image(self, argc, argv, SharpenImage);
}


/*
 *  Method:     Image#sharpen_channel(radius=0, sigma=1, channel=AllChannels)
 *  Returns:    a new image
*/
VALUE
Image_sharpen_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_SHARPENIMAGECHANNEL)
    Image *image, *new_image;
    ChannelType channels;
    ExceptionInfo exception;
    double radius = 0.0, sigma = 1.0;

    channels = extract_channels(&argc, argv);

    // There must be 0, 1, or 2 remaining arguments.
    switch(argc)
    {
        case 2:
             sigma = NUM2DBL(argv[1]);
             /* Fall thru */
        case 1:
             radius = NUM2DBL(argv[0]);
             /* Fall thru */
        case 0:
             break;
        default:
            raise_ChannelType_error(argv[argc-1]);
    }

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    GetExceptionInfo(&exception);
    (void) SharpenImageChannel(new_image, channels, radius, sigma, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#shave(width, height)
                Image#shave!(width, height)
    Purpose:    shaves pixels from the image edges, leaving a rectangle
                of the specified width & height in the center
    Returns:    shave: a new image
                shave!: self, shaved
*/
VALUE
Image_shave(
    VALUE self,
    VALUE width,
    VALUE height)
{
    return xform_image(False, self, INT2FIX(0), INT2FIX(0), width, height, ShaveImage);
}


VALUE
Image_shave_bang(
    VALUE self,
    VALUE width,
    VALUE height)
{
    rm_check_frozen(self);
    return xform_image(True, self, INT2FIX(0), INT2FIX(0), width, height, ShaveImage);
}


/*
    Method:     Image#shear(x_shear, y_shear)
    Purpose:    Calls ShearImage
    Notes:      shear angles are measured in degrees
    Returns:    a new image
*/
VALUE
Image_shear(
    VALUE self,
    VALUE x_shear,
    VALUE y_shear)
{
    Image *image, *new_image;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = ShearImage(image, NUM2DBL(x_shear), NUM2DBL(y_shear), &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}


/*
 *  Method: Image#sigmoidal_contrast_channel(contrast=3.0, midpoint=50.0,
                        sharpen=false [, channel=AllChannels]);
*/
VALUE
Image_sigmoidal_contrast_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_SIGMOIDALCONTRASTIMAGECHANNEL)
    Image *image, *new_image;
    MagickBooleanType sharpen = MagickFalse;
    double contrast = 3.0;
    double midpoint = 50.0;
    ChannelType channels;

    Data_Get_Struct(self, Image, image);

    channels = extract_channels(&argc, argv);

    switch(argc)
    {
        case 3:
            sharpen  = (MagickBooleanType) RTEST(argv[2]);
        case 2:
            midpoint = NUM2DBL(argv[1]);
        case 1:
            contrast = NUM2DBL(argv[0]);
        case 0:
            break;
        default:
            raise_ChannelType_error(argv[argc-1]);
            break;
    }

    new_image = rm_clone_image(image);

    (void) SigmoidalContrastImageChannel(new_image, channels, sharpen, contrast, midpoint);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:     Image#signature
    Purpose:    computes a message digest from an image pixel stream with an
                implementation of the NIST SHA-256 Message Digest algorithm.
*/
VALUE
Image_signature(VALUE self)
{
    Image *image;
    const ImageAttribute *signature;

    Data_Get_Struct(self, Image, image);
    (void) SignatureImage(image);
    signature = GetImageAttribute(image, "signature");
    if (!signature)
    {
        return Qnil;
    }
    return rb_str_new(signature->value, 64);
}



/*
    Method:     Image#sketch(radius=0.0, sigma=1.0, angle=0.0)
    Purpose:    Call SketchImage
*/
VALUE
Image_sketch(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_SKETCHIMAGE)
    return motion_blur(argc, argv, self, SketchImage);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#solarize(threshold=50.0)
    Purpose:    applies a special effect to the image, similar to the effect
                achieved in a photo darkroom by selectively exposing areas of
                photo sensitive paper to light. Threshold ranges from 0 to
                MaxRGB and is a measure of the extent of the solarization.
*/
VALUE
Image_solarize(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double threshold = 50.0;

    switch (argc)
    {
        case 1:
            threshold = NUM2DBL(argv[0]);
            if (threshold < 0.0 || threshold > MaxRGB)
            {
                rb_raise(rb_eArgError, "threshold out of range, must be >= 0.0 and < MaxRGB");
            }
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 or 1)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    (void) SolarizeImage(new_image, threshold);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}

/*
    Method:     Image#spaceship     (a <=> b)
    Purpose:    compare two images
    Returns:    -1, 0, 1
*/
VALUE
Image_spaceship(VALUE self, VALUE other)
{
    Image *imageA, *imageB;
    const ImageAttribute *sigA, *sigB;
    int res;

    // If the other object isn't a Image object, then they can't be equal.
    if (!rb_obj_is_kind_of(other, Class_Image))
    {
#if RUBY_VERSION < 0x180
        // In 1.6, raise TypeError
        rb_raise(rb_eTypeError, "%s compared with %s",
                                rb_class2name(CLASS_OF(self)),
                                rb_class2name(CLASS_OF(other)));
#else
        // In 1.8, return nil
        return Qnil;
#endif
    }

    Data_Get_Struct(self, Image, imageA);
    Data_Get_Struct(other, Image, imageB);

    (void) SignatureImage(imageA);
    (void) SignatureImage(imageB);
    sigA = GetImageAttribute(imageA, "signature");
    sigB = GetImageAttribute(imageB, "signature");
    if (!sigA || !sigB)
    {
        rb_raise(Class_ImageMagickError, "can't get image signature");
    }

    res = memcmp(sigA->value, sigB->value, 64);
    res = res > 0 ? 1 : (res < 0 ? -1 :  0);    // reduce to 1, -1, 0

    return INT2FIX(res);
}

/*
    Method:     Image#splice(x, y, width, height[, color])
    Purpose:    Splice a solid color into the part of the image specified
                by the x, y, width, and height arguments. If the color
                argument is specified it must be a color name or Pixel.
                If not specified uses the background color.
    Notes:      splice is the inverse of chop
*/

VALUE
Image_splice(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_SPLICEIMAGE)
    Image *image, *new_image;
    PixelPacket color, old_color;
    RectangleInfo rectangle;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    switch(argc)
    {
        case 4:
            // use background color
            color = image->background_color;
            break;
        case 5:
            // Convert color argument to PixelPacket
            Color_to_PixelPacket(&color, argv[4]);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 4 or 5)", argc);
            break;
    }

    rectangle.x      = NUM2LONG(argv[0]);
    rectangle.y      = NUM2LONG(argv[1]);
    rectangle.width  = NUM2ULONG(argv[2]);
    rectangle.height = NUM2ULONG(argv[3]);

    GetExceptionInfo(&exception);

    // Swap in color for the duration of this call.
    old_color = image->background_color;
    image->background_color = color;
    new_image = SpliceImage(image, &rectangle, &exception);
    image->background_color = old_color;

    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);

#else

    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:     Image#spread(radius=3)
    Purpose:    randomly displaces each pixel in a block defined by "radius"
    Returns:    a new image
*/
VALUE
Image_spread(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    unsigned int radius = 3;
    ExceptionInfo exception;

    switch (argc)
    {
        case 1:
            radius = NUM2UINT(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 or 1)", argc);
            break;
    }
    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = SpreadImage(image, radius, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);
    rm_ensure_result(new_image);

    (void) DestroyExceptionInfo(&exception);

    return rm_image_new(new_image);
}

DEF_ATTR_ACCESSOR(Image, start_loop, bool)


/*
    Method:     Image#statistics
    Notes:      Only supported in GM 1.1
*/
VALUE
Image_statistics(VALUE self)
{
#if defined(HAVE_GETIMAGESTATISTICS)
    Image *image;
    ExceptionInfo exception;
    ImageStatistics stats;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    (void) GetImageStatistics(image, &stats, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return Statistics_new(&stats);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#stegano(watermark, offset)
    Purpose:    hides a digital watermark within the image. Recover the hidden
                watermark later to prove that the authenticity of an image.
                "Offset" is the start position within the image to hide the
                watermark.
    Returns:    a new image
*/
VALUE
Image_stegano(
    VALUE self,
    VALUE watermark_image,
    VALUE offset)
{
    Image *image, *new_image;
    volatile VALUE wm_image;
    Image *watermark;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    wm_image = ImageList_cur_image(watermark_image);
    Data_Get_Struct(wm_image, Image, watermark);

    image->offset = NUM2LONG(offset);

    GetExceptionInfo(&exception);
    new_image = SteganoImage(image, watermark, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Method:     Image#stereo(offset_image)
    Purpose:    combines two images and produces a single image that is the
                composite of a left and right image of a stereo pair.
                Special red-green stereo glasses are required to view this
                effect.
    Returns:    a new image
*/
VALUE
Image_stereo(
    VALUE self,
    VALUE offset_image_arg)
{
    Image *image, *new_image;
    volatile VALUE offset_image;
    Image *offset;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    offset_image = ImageList_cur_image(offset_image_arg);
    Data_Get_Struct(offset_image, Image, offset);

    GetExceptionInfo(&exception);
    new_image = StereoImage(image, offset, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Method:     Image#class_type
    Purpose:    return the image's storage class (a.k.a. storage type, class type)
    Notes:      based on Magick++'s Magick::Magick::classType
*/
VALUE
Image_class_type(VALUE self)
{
    Image *image;
    Data_Get_Struct(self, Image, image);

    return ClassType_new(image->storage_class);
}

/*
    Method:     Image#class_type=
    Purpose:    change the image's storage class
    Notes:      based on Magick++'s Magick::Magick::classType
*/
VALUE
Image_class_type_eq(VALUE self, VALUE new_class_type)
{
    Image *image;
    ClassType class_type;
    QuantizeInfo qinfo;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    VALUE_TO_ENUM(new_class_type, class_type, ClassType);

    if (image->storage_class == PseudoClass && class_type == DirectClass)
    {
        (void) SyncImage(image);
        magick_free(image->colormap);
        image->colormap = NULL;
    }
    else if (image->storage_class == DirectClass && class_type == PseudoClass)
    {
        GetQuantizeInfo(&qinfo);
        qinfo.number_colors = MaxRGB+1;
        (void) QuantizeImage(&qinfo, image);
    }

#if defined(HAVE_SETIMAGESTORAGECLASS)
    (void) SetImageStorageClass(image, class_type);
#else
    (void) SyncImage(image);
    image->storage_class = class_type;
#endif
    return self;
}

/*
    Method:     Image#store_pixels
    Purpose:    Replace the pixels in the specified rectangle
    Notes:      Calls GetImagePixels, then SyncImagePixels after replacing
                the pixels. This is the complement of get_pixels. The array
                object returned by get_pixels is suitable for use as the
                "new_pixels" argument.
*/
VALUE
Image_store_pixels(
    VALUE self,
    VALUE x_arg,
    VALUE y_arg,
    VALUE cols_arg,
    VALUE rows_arg,
    VALUE new_pixels)
{
    Image *image;
    Pixel *pixels, *pixel;
    volatile VALUE new_pixel;
    long n, size;
    long x, y;
    unsigned long cols, rows;
    unsigned int okay;

    Data_Get_Struct(self, Image, image);

    x = NUM2LONG(x_arg);
    y = NUM2LONG(y_arg);
    cols = NUM2ULONG(cols_arg);
    rows = NUM2ULONG(rows_arg);
    if (x < 0 || y < 0 || x+cols > image->columns || y+rows > image->rows)
    {
        rb_raise(rb_eRangeError, "geometry (%lux%lu%+ld%+ld) exceeds image bounds"
               , cols, rows, x, y);
    }

    size = (long)(cols * rows);
    rm_check_ary_len(new_pixels, size);

#if defined(HAVE_SETIMAGESTORAGECLASS)
    okay = SetImageStorageClass(image,DirectClass);
    rm_check_image_exception(image, RetainOnError);
    if (!okay)
    {
        rb_raise(Class_ImageMagickError, "Can't store pixels. Can't change image storage class.");
    }
#else
    if (image->storage_class == PseudoClass)
    {
        SyncImage(image);
        rm_check_image_exception(image, RetainOnError);
        image->storage_class = DirectClass;
    }
#endif

    // Get a pointer to the pixels. Replace the values with the PixelPackets
    // from the pixels argument.
    pixels = GetImagePixels(image, x, y, cols, rows);
    rm_check_image_exception(image, RetainOnError);

    if (pixels)
    {
        for (n = 0; n < size; n++)
        {
            new_pixel = rb_ary_entry(new_pixels, n);
            Data_Get_Struct(new_pixel, Pixel, pixel);
            pixels[n] = *pixel;
        }

        okay = SyncImagePixels(image);
        rm_check_image_exception(image, RetainOnError);
        if (!okay)
        {
            rb_raise(Class_ImageMagickError, "Can't store pixels. Can't sync image pixels.");
        }
    }

    return self;
}


/*
    Method:     Image#strip!
    Purpose:    strips an image of all profiles and comments.
*/
VALUE
Image_strip_bang(VALUE self)
{
#if defined(HAVE_STRIPIMAGE)
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    (void) StripImage(image);
    return self;
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#swirl(degrees)
    Purpose:    swirls the pixels about the center of the image, where degrees
                indicates the sweep of the arc through which each pixel is moved.
                You get a more dramatic effect as the degrees move from 1 to 360.
*/
VALUE
Image_swirl(VALUE self, VALUE degrees)
{
    Image *image, *new_image;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = SwirlImage(image, NUM2DBL(degrees), &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}

/*
    Method:     Image#texture_flood_fill(color, texture, x, y, method)
    Purpose:    Emulates Magick++'s floodFillTexture
                If the FloodfillMethod method is specified, flood-fills
                texture across pixels starting at the target pixel and
                matching the specified color.

                If the FillToBorderMethod method is specified, flood-fills
                "texture across pixels starting at the target pixel and
                stopping at pixels matching the specified color."
*/
VALUE
Image_texture_flood_fill(
    VALUE self,
    VALUE color_obj,
    VALUE texture_obj,
    VALUE x_obj,
    VALUE y_obj,
    VALUE method_obj)
{
    Image *image, *new_image;
    Image *texture_image;
    PixelPacket color;
    volatile VALUE texture;
    DrawInfo *draw_info;
    long x, y;
    PaintMethod method;

    Data_Get_Struct(self, Image, image);
    Color_to_PixelPacket(&color, color_obj);
    texture = ImageList_cur_image(texture_obj);
    x = NUM2LONG(x_obj);
    y = NUM2LONG(y_obj);

    if ((unsigned long)x > image->columns || (unsigned long)y > image->rows)
    {
        rb_raise(rb_eArgError, "target out of range. %ldx%ld given, image is %lux%lu"
               , x, y, image->columns, image->rows);
    }

    VALUE_TO_ENUM(method_obj, method, PaintMethod);
    if (method != FillToBorderMethod && method != FloodfillMethod)
    {
        rb_raise(rb_eArgError, "paint method must be FloodfillMethod or "
                               "FillToBorderMethod (%d given)", (int)method);
    }

    draw_info = CloneDrawInfo(NULL, NULL);
    if (!draw_info)
    {
        rb_raise(rb_eNoMemError, "not enough memory to continue");
    }
    Data_Get_Struct(texture, Image, texture_image);

    draw_info->fill_pattern = rm_clone_image(texture_image);
    new_image = rm_clone_image(image);

    // Hack: By-pass bug in ColorFloodfillImage that tests
    // the fill color even though the fill color isn't used.
    if (method == FillToBorderMethod)
    {
        draw_info->fill.red = ROUND_TO_QUANTUM(color.red + new_image->fuzz + 1);
        draw_info->fill.green = color.green;
        draw_info->fill.blue = color.blue;
    }

    (void) ColorFloodfillImage(new_image, draw_info, color, x, y, method);

    (void) DestroyDrawInfo(draw_info);
    rm_check_image_exception(new_image, DestroyOnError);


    return rm_image_new(new_image);
}

/*
    Method:     Image#threshold(threshold)
    Purpose:    changes the value of individual pixels based on the intensity of
                each pixel compared to threshold. The result is a high-contrast,
                two color image.
*/
VALUE
Image_threshold(VALUE self, VALUE threshold)
{
    Image *image, *new_image;

    Data_Get_Struct(self, Image, image);

    new_image = rm_clone_image(image);

    (void) ThresholdImage(new_image, NUM2DBL(threshold));
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}


/*
 *  Static:     threshold_image
 *  Purpose:    call one of the xxxxThresholdImage methods
*/
static
VALUE threshold_image(
    int argc,
    VALUE *argv,
    VALUE self,
    thresholder_t thresholder)
{
    Image *image, *new_image;
    double red, green, blue, opacity;
    char ctarg[200];

    Data_Get_Struct(self, Image, image);

    switch (argc)
    {
        case 4:
            red     = NUM2DBL(argv[0]);
            green   = NUM2DBL(argv[1]);
            blue    = NUM2DBL(argv[2]);
            opacity = NUM2DBL(argv[3]);
            sprintf(ctarg, "%f,%f,%f,%f", red, green, blue, opacity);
            break;
        case 3:
            red     = NUM2DBL(argv[0]);
            green   = NUM2DBL(argv[1]);
            blue    = NUM2DBL(argv[2]);
            sprintf(ctarg, "%f,%f,%f", red, green, blue);
            break;
        case 2:
            red     = NUM2DBL(argv[0]);
            green   = NUM2DBL(argv[1]);
            sprintf(ctarg, "%f,%f", red, green);
            break;
        case 1:
            red     = NUM2DBL(argv[0]);
            sprintf(ctarg, "%f", red);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 to 4)", argc);
    }

    new_image = rm_clone_image(image);

    (thresholder)(new_image, ctarg);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}


/*
    Method:     Image#thumbnail(scale) or (cols, rows)
                Image#thumbnail!(scale) or (cols, rows)
    Purpose:    fast resize for thumbnail images
    Returns:    a resized copy of the input image
    Notes:      Uses BoxFilter, blur attribute of input image
*/
static VALUE
thumbnail(int bang, int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    unsigned long columns, rows;
    double scale, drows, dcols;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    switch (argc)
    {
        case 2:
            columns = NUM2ULONG(argv[0]);
            rows = NUM2ULONG(argv[1]);
            if (columns == 0 || rows == 0)
            {
                rb_raise(rb_eArgError, "invalid result dimension (%lu, %lu given)", columns, rows);
            }
            break;
        case 1:
            scale = NUM2DBL(argv[0]);
            if (scale < 0.0)
            {
                rb_raise(rb_eArgError, "invalid scale value (%g given)", scale);
            }
            drows = scale * image->rows + 0.5;
            dcols = scale * image->columns + 0.5;
            if (drows > (double)ULONG_MAX || dcols > (double)ULONG_MAX)
            {
                rb_raise(rb_eRangeError, "resized image too big");
            }
            rows = (unsigned long) drows;
            columns = (unsigned long) dcols;
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 or 2)", argc);
            break;
    }

    GetExceptionInfo(&exception);
    new_image = ThumbnailImage(image, columns, rows, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    if (bang)
    {
        DATA_PTR(self) = new_image;
        (void) DestroyImage(image);
        return self;
    }

    return rm_image_new(new_image);
}

VALUE
Image_thumbnail(int argc, VALUE *argv, VALUE self)
{
    return thumbnail(False, argc, argv, self);
}

VALUE
Image_thumbnail_bang(int argc, VALUE *argv, VALUE self)
{
    rm_check_frozen(self);
    return thumbnail(True, argc, argv, self);
}


/*
    Method:     Image#ticks_per_second, ticks_per_second=
    Purpose:    the ticks_per_second attribute accessors
*/
VALUE
Image_ticks_per_second(VALUE self)
{
#if defined(HAVE_IMAGE_TICKS_PER_SECOND)
    Image *image;

    Data_Get_Struct(self, Image, image);
    return INT2FIX(image->ticks_per_second);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


VALUE
Image_ticks_per_second_eq(VALUE self, VALUE tps)
{
#if defined(HAVE_IMAGE_TICKS_PER_SECOND)
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    image->ticks_per_second = NUM2ULONG(tps);
    return self;
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#tile_info, tile_info=
    Purpose:    the tile_info attribute accessors
*/
VALUE
Image_tile_info(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
#ifdef HAVE_IMAGE_EXTRACT_INFO
    // Deprecated in 5.5.6 and later
    rb_warning("RMagick: tile_info is deprecated in this release of ImageMagick. Use extract_info instead.");
    return Rectangle_from_RectangleInfo(&image->extract_info);
#else
    return Rectangle_from_RectangleInfo(&image->tile_info);
#endif
}

VALUE
Image_tile_info_eq(VALUE self, VALUE rect)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
#ifdef HAVE_IMAGE_EXTRACT_INFO
    // Deprecated in 5.5.6 and later
    rb_warning("RMagick: tile_info= is deprecated in this release of ImageMagick. Use extract_info= instead.");
    rm_check_frozen(self);
    Rectangle_to_RectangleInfo(&image->extract_info, rect);
#else
    rm_check_frozen(self);
    Rectangle_to_RectangleInfo(&image->tile_info, rect);
#endif
    return self;
}


/*
    Method:     Image#tint
    Purpose:    Call TintImage
    Notes:      New for 5.5.8
                Opacity values are percentages: 0.10 -> 10%.
*/
VALUE
Image_tint(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_TINTIMAGE)
    Image *image, *new_image;
    Pixel *tint;
    double red_pct_opaque, green_pct_opaque, blue_pct_opaque;
    double alpha_pct_opaque = 1.0;
    char opacity[50];
    ExceptionInfo exception;

    switch(argc)
    {
        case 2:
            red_pct_opaque   = NUM2DBL(argv[1]);
            green_pct_opaque = blue_pct_opaque = red_pct_opaque;
            break;
        case 3:
            red_pct_opaque   = NUM2DBL(argv[1]);
            green_pct_opaque = NUM2DBL(argv[2]);
            blue_pct_opaque  = red_pct_opaque;
            break;
        case 4:
            red_pct_opaque     = NUM2DBL(argv[1]);
            green_pct_opaque   = NUM2DBL(argv[2]);
            blue_pct_opaque    = NUM2DBL(argv[3]);
            break;
        case 5:
            red_pct_opaque     = NUM2DBL(argv[1]);
            green_pct_opaque   = NUM2DBL(argv[2]);
            blue_pct_opaque    = NUM2DBL(argv[3]);
            alpha_pct_opaque   = NUM2DBL(argv[4]);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 to 5)", argc);
            break;
    }

    if (red_pct_opaque < 0.0 || green_pct_opaque < 0.0
        || blue_pct_opaque < 0.0 || alpha_pct_opaque < 0.0)
    {
        rb_raise(rb_eArgError, "opacity percentages must be non-negative.");
    }

#if defined(HAVE_SNPRINTF)
    snprintf(opacity, sizeof(opacity),
#else
    sprintf(opacity,
#endif
                     "%g,%g,%g,%g", red_pct_opaque*100.0, green_pct_opaque*100.0
                   , blue_pct_opaque*100.0, alpha_pct_opaque*100.0);

    Data_Get_Struct(argv[0], Pixel, tint);
    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = TintImage(image, opacity, *tint, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

/*
    Method:     Image#to_blob
    Purpose:    Return a "blob" (a String) from the image
    Notes:      The magick member of the Image structure
                determines the format of the returned blob
                (GIG, JPEG,  PNG, etc.)
*/
VALUE
Image_to_blob(VALUE self)
{
    Image *image;
    Info *info;
    const MagickInfo *magick_info;
    volatile VALUE info_obj;
    volatile VALUE blob_str;
    void *blob = NULL;
    size_t length = 2048;       // Do what Magick++ does
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    // The user can specify the depth (8 or 16, if the format supports
    // both) and the image format by setting the depth and format
    // values in the info parm block.
    info_obj = rm_info_new();
    Data_Get_Struct(info_obj, Info, info);

    Data_Get_Struct(self, Image, image);

    // Copy the depth and magick fields to the Image
    if (info->depth != 0)
    {
        (void) SetImageDepth(image, info->depth);
        rm_check_image_exception(image, RetainOnError);
    }

    GetExceptionInfo(&exception);
    if (*info->magick)
    {
        (void) SetImageInfo(info, MagickTrue, &exception);
        CHECK_EXCEPTION()

        if (*info->magick == '\0')
        {
            return Qnil;
        }
        strncpy(image->magick, info->magick, sizeof(info->magick)-1);
    }

    // Fix #2844 - libjpeg exits when image is 0x0
    magick_info = GetMagickInfo(image->magick, &exception);
    CHECK_EXCEPTION()

    if (magick_info)
    {
        if (  (!rm_strcasecmp(magick_info->name, "JPEG")
            || !rm_strcasecmp(magick_info->name, "JPG"))
            && (image->rows == 0 || image->columns == 0))
        {
            rb_raise(rb_eRuntimeError, "Can't convert %lux%lu %.4s image to a blob"
                   , image->columns, image->rows, magick_info->name);
        }
    }

    blob = ImageToBlob(info, image, &length, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    if (length == 0 || !blob)
    {
        return Qnil;
    }

    blob_str = rb_str_new(blob, length);

    magick_free((void*)blob);

    return blob_str;
}

/*
    Method:     Image#to_color
    Purpose:    Return a color name for the color intensity specified by the
                Magick::Pixel argument.
    Notes:      Respects depth and matte attributes
*/
VALUE
Image_to_color(VALUE self, VALUE pixel_arg)
{
    Image *image;
    Pixel *pixel;
    ExceptionInfo exception;
    char name[MaxTextExtent];

    Data_Get_Struct(pixel_arg, Pixel, pixel);
    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    // QueryColorname returns False if the color represented by the PixelPacket
    // doesn't have a "real" name, just a sequence of hex digits. We don't care
    // about that.

    name[0] = '\0';
    (void) QueryColorname(image, pixel, AllCompliance, name, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return rb_str_new2(name);

}

/*
    Method:     Image#total_colors
    Purpose:    alias for Image#number_colors
    Notes:      This used to be a direct reference to the `total_colors' field in Image
                but that field is not reliable.
*/
VALUE
Image_total_colors(VALUE self)
{
    return Image_number_colors(self);
}

/*
    Method:     Image#transparent(color-name<, opacity>)
                Image#transparent(pixel<, opacity>)
    Purpose:    Call TransparentImage
    Notes:      Can use Magick::OpaqueOpacity or Magick::TransparentOpacity,
                or any value >= 0 && <= MaxRGB. The default is
                Magick::TransparentOpacity.
                Use Image#fuzz= to define the tolerance level.
*/
VALUE
Image_transparent(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    PixelPacket color;
    unsigned int opacity = TransparentOpacity;

    Data_Get_Struct(self, Image, image);

    switch (argc)
    {
        case 2:
            opacity = NUM2UINT(argv[1]);
            if (opacity > TransparentOpacity)
            {
                opacity = TransparentOpacity;
            }
        case 1:
            Color_to_PixelPacket(&color, argv[0]);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 1 or 2)", argc);
            break;
    }

    new_image = rm_clone_image(image);

    (void) TransparentImage(new_image, color, (Quantum)opacity);
    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}


/*
    Method:     Image#transparent_color
    Purpose:    Return the name of the transparent color as a String.
*/
VALUE
Image_transparent_color(VALUE self)
{
#if defined(HAVE_IMAGE_TRANSPARENT_COLOR)
    Image *image;

    Data_Get_Struct(self, Image, image);
    return PixelPacket_to_Color_Name(image, &image->transparent_color);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#transparent_color=
    Purpose:    Set the the transparent color to the specified color spec.
*/
VALUE
Image_transparent_color_eq(VALUE self, VALUE color)
{
#if defined(HAVE_IMAGE_TRANSPARENT_COLOR)
    Image *image;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    Color_to_PixelPacket(&image->transparent_color, color);
    return self;
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
 *  Method:     Image#transpose
 *              Image#transpose!
 *  Purpose:    Call TransposeImage
 */
VALUE
Image_transpose(VALUE self)
{
#if defined(HAVE_TRANSPOSEIMAGE)
    return crisscross(False, self, TransposeImage);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


VALUE
Image_transpose_bang(VALUE self)
{
#if defined(HAVE_TRANSPOSEIMAGE)
    rm_check_frozen(self);
    return crisscross(True, self, TransposeImage);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
 *  Method:     Image#transverse
 *              Image#transverse!
 *  Purpose:    Call TransverseImage
 */
VALUE
Image_transverse(VALUE self)
{
#if defined(HAVE_TRANSVERSEIMAGE)
    return crisscross(False, self, TransverseImage);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}

VALUE
Image_transverse_bang(VALUE self)
{
#if defined(HAVE_TRANSVERSEIMAGE)
    rm_check_frozen(self);
    return crisscross(True, self, TransverseImage);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
 *  Method:     Image#trim, Image#trim!
 *  Purpose:    convenience front-end to CropImage
 *  Notes:      respects fuzz attribute
 */

static VALUE
trimmer(int bang, VALUE self)
{
    Image *image, *new_image;
    RectangleInfo geometry;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    GetExceptionInfo(&exception);

    geometry = GetImageBoundingBox(image, &exception);
    CHECK_EXCEPTION()

    new_image = CropImage(image, &geometry, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    if (bang)
    {
        DATA_PTR(self) = new_image;
        (void) DestroyImage(image);
        return self;
    }

    return rm_image_new(new_image);
}


VALUE
Image_trim(VALUE self)
{
    return trimmer(False, self);
}

VALUE
Image_trim_bang(VALUE self)
{
    return trimmer(True, self);
}



/*
    Method:     Image#image_type=(type)
    Purpose:    Call SetImageType to set the type of the image
    Note:       Can't use type & type= b/c of Object#type.
                This setter is useless. Leave for backward compatibility
                but don't document it.
*/
VALUE Image_image_type_eq(VALUE self, VALUE type)
{
    Image *image;
    ImageType it;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    VALUE_TO_ENUM(type, it, ImageType);
    (void) SetImageType(image, it);

    return self;
}

/*
    Method:     Image#image_type
    Purpose:    Call GetImageType to get the image type
*/
VALUE Image_image_type(VALUE self)
{
    Image *image;
    ImageType type;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);
    type = GetImageType(image, &exception);
    CHECK_EXCEPTION()

    (void) DestroyExceptionInfo(&exception);

    return ImageType_new(type);
}


/*
    Method:     Image#unique_colors
    Purpose:    Call UniqueImageColors
*/
VALUE
Image_unique_colors(VALUE self)
{
#if defined(HAVE_UNIQUEIMAGECOLORS)
    Image *image, *new_image;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = UniqueImageColors(image, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);
    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
    Method:     Image#units
    Purpose:    Get the resolution type field
*/
VALUE
Image_units(VALUE self)
{
    Image *image;

    Data_Get_Struct(self, Image, image);
    return ResolutionType_new(image->units);
}

/*
    Method:     Image#units=
    Purpose:    Set the resolution type field
*/
VALUE
Image_units_eq(
    VALUE self,
    VALUE restype)
{
    Image *image;
    ResolutionType units;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);

    VALUE_TO_ENUM(restype, image->units, ResolutionType);

    if (image->units != units)
    {
        switch (image->units)
        {
            case PixelsPerInchResolution:
                if (units == PixelsPerCentimeterResolution)
                {
                    image->x_resolution /= 2.54;
                    image->y_resolution /= 2.54;
                }
                break;

            case PixelsPerCentimeterResolution:
                if (units == PixelsPerInchResolution)
                {
                  image->x_resolution *= 2.54;
                  image->y_resolution *= 2.54;
                }
                break;

            default:
                // UndefinedResolution
                image->x_resolution = 0.0;
                image->y_resolution = 0.0;
                break;
        }

        image->units = units;
    }

    return self;
}


/*
    Method:     Image#unsharp_mask(radius=0.0, sigma=1.0, amount=1.0, threshold=0.05)
    Purpose:    sharpens an image. "amount" is the percentage of the difference
                between the original and the blur image that is added back into
                the original. "threshold" is the threshold in pixels needed to
                apply the diffence amount.
*/
static void
unsharp_mask_args(
    int argc,
    VALUE *argv,
    double *radius,
    double *sigma,
    double *amount,
    double *threshold)
{
    switch (argc)
    {
        case 4:
            *threshold = NUM2DBL(argv[3]);
            if (*threshold < 0.0)
            {
                rb_raise(rb_eArgError, "threshold must be >= 0.0");
            }
        case 3:
            *amount = NUM2DBL(argv[2]);
            if (*amount <= 0.0)
            {
                rb_raise(rb_eArgError, "amount must be > 0.0");
            }
        case 2:
            *sigma = NUM2DBL(argv[1]);
            if (*sigma == 0.0)
            {
                rb_raise(rb_eArgError, "sigma must be != 0.0");
            }
        case 1:
            *radius = NUM2DBL(argv[0]);
            if (*radius < 0.0)
            {
                rb_raise(rb_eArgError, "radius must be >= 0.0");
            }
        case 0:
            break;

            // This case can't occur if we're called from Image_unsharp_mask_channel
            // because it has already raised an exception for the the argc > 4 case.
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 4)", argc);
    }
}


VALUE
Image_unsharp_mask(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double radius = 0.0, sigma = 1.0, amount = 1.0, threshold = 0.05;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    unsharp_mask_args(argc, argv, &radius, &sigma, &amount, &threshold);

    GetExceptionInfo(&exception);
    new_image = UnsharpMaskImage(image, radius, sigma, amount, threshold, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}


/*
    Method:     Image#unsharp_mask_channel(radius, sigma, amount,threshold,
                                           channel=AllChannels)
    Purpose:    Call UnsharpMaskImageChannel
*/
VALUE
Image_unsharp_mask_channel(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_UNSHARPMASKIMAGECHANNEL)
    Image *image, *new_image;
    ChannelType channels;
    double radius = 0.0, sigma = 1.0, amount = 1.0, threshold = 0.05;
    ExceptionInfo exception;

    channels = extract_channels(&argc, argv);
    if (argc > 4)
    {
        raise_ChannelType_error(argv[argc-1]);
    }

    unsharp_mask_args(argc, argv, &radius, &sigma, &amount, &threshold);

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);
    new_image = UnsharpMaskImageChannel(image, channels, radius, sigma, amount
                                      , threshold, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
  Method:   Image#vignette(horz_radius, vert_radius, radius, sigma);
  Purpose:  soften the edges of an image
  Notes:    The outer edges of the image are replaced by the background color.
*/
VALUE
Image_vignette(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_VIGNETTEIMAGE)
    Image *image, *new_image;
    long horz_radius, vert_radius;
    double radius = 0.0, sigma = 10.0;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    horz_radius = (long)(image->columns * 0.10 + 0.5);
    vert_radius = (long)(image->rows * 0.10 + 0.5);

    switch (argc)
    {
        case 4:
            sigma = NUM2DBL(argv[3]);
        case 3:
            radius = NUM2DBL(argv[2]);
        case 2:
            vert_radius = NUM2INT(argv[1]);
        case 1:
            horz_radius = NUM2INT(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 4)", argc);
            break;
    }

    GetExceptionInfo(&exception);

    new_image = VignetteImage(image, radius, sigma, horz_radius, vert_radius, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);

#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
  Method:   Image#virtual_pixel_method
  Purpose:  get the VirtualPixelMethod for the image
*/
VALUE
Image_virtual_pixel_method(VALUE self)
{
    Image *image;
    VirtualPixelMethod vpm;

    Data_Get_Struct(self, Image, image);
    vpm = GetImageVirtualPixelMethod(image);

    return VirtualPixelMethod_new(vpm);
}


/*
  Method:   Image#virtual_pixel_method=
  Purpose:  set the virtual pixel method for the image
*/
VALUE
Image_virtual_pixel_method_eq(VALUE self, VALUE method)
{
    Image *image;
    VirtualPixelMethod vpm;

    rm_check_frozen(self);
    Data_Get_Struct(self, Image, image);
    VALUE_TO_ENUM(method, vpm, VirtualPixelMethod);
    (void) SetImageVirtualPixelMethod(image, vpm);
    return self;
}




/*
  Method:   Image#watermark(mark, brightness=100.0, saturation=100.0
                          , [gravity,] x_off=0, y_off=0)
  Purpose:  add a watermark to an image
  Notes:    x_off and y_off can be negative, which means measure from the right/bottom
            of the target image.
*/
VALUE
Image_watermark(int argc, VALUE *argv, VALUE self)
{
    Image *image, *overlay, *new_image;
    double src_percent = 100.0, dst_percent = 100.0;
    long x_offset = 0L, y_offset = 0L;
    char geometry[20];

    Data_Get_Struct(self, Image, image);

    if (argc < 1)
    {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 to 6)", argc);
    }

    if (argc > 3)
    {
        Data_Get_Struct(ImageList_cur_image(argv[0]), Image, overlay);
        get_composite_offsets(argc-3, &argv[3], image, overlay, &x_offset, &y_offset);
        // There must be 3 arguments left
        argc = 3;
    }

    switch (argc)
    {
        case 3:
            dst_percent = rm_percentage(argv[2]) * 100.0;
        case 2:
            src_percent = rm_percentage(argv[1]) * 100.0;
        case 1:
            Data_Get_Struct(ImageList_cur_image(argv[0]), Image, overlay);
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 2 to 6)", argc);
            break;
    }

    blend_geometry(geometry, sizeof(geometry), src_percent, dst_percent);
    (void) CloneString(&overlay->geometry, geometry);

    new_image = rm_clone_image(image);
    (void) CompositeImage(new_image, ModulateCompositeOp, overlay, x_offset, y_offset);

    rm_check_image_exception(new_image, DestroyOnError);

    return rm_image_new(new_image);
}

/*
  Method:   Image#wave(amplitude=25.0, wavelength=150.0)
  Purpose:  creates a "ripple" effect in the image by shifting the pixels
            vertically along a sine wave whose amplitude and wavelength is
            specified by the given parameters.
  Returns:  self
*/
VALUE
Image_wave(int argc, VALUE *argv, VALUE self)
{
    Image *image, *new_image;
    double amplitude = 25.0, wavelength = 150.0;
    ExceptionInfo exception;

    switch (argc)
    {
        case 2:
            wavelength = NUM2DBL(argv[1]);
        case 1:
            amplitude = NUM2DBL(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 2)", argc);
            break;
    }

    Data_Get_Struct(self, Image, image);
    GetExceptionInfo(&exception);

    new_image = WaveImage(image, amplitude, wavelength, &exception);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    return rm_image_new(new_image);
}


/*
    Method:     Image#wet_floor(initial, rate)
    Purpose:    Construct a "wet floor" reflection.
    Notes:      `initial' is a number between 0 and 1, inclusive, that represents
                the initial level of transparency. Smaller numbers are less
                transparent than larger numbers. 0 is fully opaque. 1.0 is
                fully transparent. The default is 0.5.
                `rate' is the rate at which the initial level of transparency
                changes to complete transparency. The default is 1.0. Values
                larger than 1.0 cause the change to occur more rapidly. The
                resulting reflection will be shorter. Values smaller than 1.0
                cause the change to occur less rapidly. The resulting
                reflection will be taller. If the rate is exactly 0 then the
                amount of transparency doesn't change at all.
    Notes:      http://en.wikipedia.org/wiki/Wet_floor_effect
*/
VALUE
Image_wet_floor(int argc, VALUE *argv, VALUE self)
{
    Image *image, *reflection, *flip_image;
    PixelPacket *p, *q;
    RectangleInfo geometry;
    long x, y, max_rows;
    double initial = 0.5;
    double rate = 1.0;
    double opacity, step;
    char *func;
    ExceptionInfo exception;

    switch (argc)
    {
        case 2:
            rate = NUM2DBL(argv[1]);
        case 1:
            initial = NUM2DBL(argv[0]);
        case 0:
            break;
        default:
            rb_raise(rb_eArgError, "wrong number of arguments (%d for 0 to 2)", argc);
            break;
    }


    if (initial < 0.0 || initial > 1.0)
    {
        rb_raise(rb_eArgError, "Initial transparency must be in the range 0.0-1.0 (%g)", initial);
    }
    if (rate < 0.0)
    {
        rb_raise(rb_eArgError, "Transparency change rate must be >= 0.0 (%g)", rate);
    }

    Data_Get_Struct(self, Image, image);

    initial *= TransparentOpacity;

    // The number of rows in which to transition from the initial level of
    // transparency to complete transparency. rate == 0.0 -> no change.
    if (rate > 0.0)
    {
        max_rows = (long)((double)image->rows) / (3.0 * rate);
        max_rows = (long)min((unsigned long)max_rows, image->rows);
        step =  (TransparentOpacity - initial) / max_rows;
    }
    else
    {
        max_rows = (long)image->rows;
        step = 0.0;
    }


    GetExceptionInfo(&exception);
    flip_image = FlipImage(image, &exception);
    CHECK_EXCEPTION();


    geometry.x = 0;
    geometry.y = 0;
    geometry.width = image->columns;
    geometry.height = max_rows;
    reflection = CropImage(flip_image, &geometry, &exception);
    (void) DestroyImage(flip_image);
    CHECK_EXCEPTION();


#if defined(HAVE_SETIMAGESTORAGECLASS)
    (void) SetImageStorageClass(reflection, DirectClass);
    rm_check_image_exception(reflection, DestroyOnError);
#else
    if (reflection->storage_class == PseudoClass)
    {
        SyncImage(reflection);
        reflection->storage_class = DirectClass;
    }
#endif


    reflection->matte = MagickTrue;
    opacity = initial;

    for (y = 0; y < max_rows; y++)
    {
        if (opacity > TransparentOpacity)
        {
            opacity = TransparentOpacity;
        }

        p = (PixelPacket *)AcquireImagePixels(reflection, 0, y, image->columns, 1, &exception);
        rm_check_exception(&exception, reflection, RetainOnError);

        q = SetImagePixels(reflection, 0, y, image->columns, 1);
        if (!q)
        {
            func = "SetImagePixels";
            goto error;
        }

        for (x = 0; x < (long) image->columns; x++)
        {
            q[x] = p[x];
            // Never make a pixel *less* transparent than it already is.
            q[x].opacity = max(q[x].opacity, (Quantum)opacity);
        }

        if (SyncImagePixels(reflection) == MagickFalse)
        {
            func = "SyncImagePixels";
            goto error;
        }

        opacity += step;
    }


    (void) DestroyExceptionInfo(&exception);
    return rm_image_new(reflection);

error:
    (void) DestroyExceptionInfo(&exception);
    (void) DestroyImage(reflection);
    rb_raise(rb_eRuntimeError, "%s failed on row %lu", func, y);
    return (VALUE)0;
}


/*
 *  Method:     Image#white_threshold(red_channel [, green_channel
 *                                    [, blue_channel [, opacity_channel]]]);
 *  Purpose:    Call WhiteThresholdImage
*/
VALUE
Image_white_threshold(int argc, VALUE *argv, VALUE self)
{
#if defined(HAVE_WHITETHRESHOLDIMAGE)
    return threshold_image(argc, argv, self, WhiteThresholdImage);
#else
    rm_not_implemented();
    return (VALUE)0;
#endif
}


/*
  Method:   Image#write(filename)
  Purpose:  Write the image to the file.
  Returns:  self
*/
VALUE
Image_write(VALUE self, VALUE file)
{
    Image *image;
    Info *info;
    volatile VALUE info_obj;
    char *filename;
    long filename_l;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);

    info_obj = rm_info_new();
    Data_Get_Struct(info_obj, Info, info);

    if (TYPE(file) == T_FILE)
    {
        OpenFile *fptr;

        // Ensure file is open - raise error if not
        GetOpenFile(file, fptr);
        rb_io_check_writable(fptr);
        SetImageInfoFile(info, GetWriteFile(fptr));
    }
    else
    {
        // Copy the filename to the Info and to the Image, then call
        // SetImageInfo. (Ref: ImageMagick's utilities/convert.c.)

        // Convert arg to string. If an exception occurs raise an error condition.
        file = rb_rescue(rb_String, file, file_arg_rescue, file);

        filename = STRING_PTR_LEN(file, filename_l);
        filename_l = min(filename_l, MaxTextExtent-1);
        memcpy(info->filename, filename, (size_t)filename_l);
        info->filename[filename_l] = '\0';
        strcpy(image->filename, info->filename);

        GetExceptionInfo(&exception);
        (void) SetImageInfo(info, MagickTrue, &exception);
        CHECK_EXCEPTION()

        (void) DestroyExceptionInfo(&exception);

        if (*info->magick == '\0')
        {
            return Qnil;
        }
        SetImageInfoFile(info, NULL);
    }

    info->adjoin = MagickFalse;
    (void) WriteImage(info, image);
    rm_check_image_exception(image, RetainOnError);

    return self;
}


DEF_ATTR_ACCESSOR(Image, x_resolution, dbl)

DEF_ATTR_ACCESSOR(Image, y_resolution, dbl)


/*
    Static:     cropper
    Purpose:    determine if the argument list is
                    x, y, width, height
                or
                    gravity, width, height
                or
                    gravity, x, y, width, height
                If the 2nd or 3rd, compute new x, y values.

                The argument list can have a trailing true, false, or nil argument.
                If present and true, after cropping reset the page fields in the image.

                Call xform_image to do the cropping.
*/
static VALUE
cropper(int bang, int argc, VALUE *argv, VALUE self)
{
    volatile VALUE x, y, width, height;
    unsigned long nx = 0, ny = 0;
    unsigned long columns, rows;
    int reset_page = 0;
    GravityType gravity;
    MagickEnum *magick_enum;
    Image *image;
    VALUE cropped;

    // Check for a "reset page" trailing argument.
    if (argc >= 1)
    {
        switch (TYPE(argv[argc-1]))
        {
            case T_TRUE:
                reset_page = 1;
                // fall thru
            case T_FALSE:
            case T_NIL:
                argc -= 1;
            default:
                break;
        }
    }

    switch (argc)
    {
        case 5:
            Data_Get_Struct(self, Image, image);

            VALUE_TO_ENUM(argv[0], gravity, GravityType);

            x      = argv[1];
            y      = argv[2];
            width  = argv[3];
            height = argv[4];

            nx      = NUM2ULONG(x);
            ny      = NUM2ULONG(y);
            columns = NUM2ULONG(width);
            rows    = NUM2ULONG(height);

            switch(gravity)
            {
                case NorthEastGravity:
                case EastGravity:
                case SouthEastGravity:
                    nx = image->columns - columns - nx;
                    break;
                case NorthGravity:
                case SouthGravity:
                case CenterGravity:
                case StaticGravity:
                    nx += image->columns/2 - columns/2;
                    break;
                default:
                    break;
            }
            switch(gravity)
            {
                case SouthWestGravity:
                case SouthGravity:
                case SouthEastGravity:
                    ny = image->rows - rows - ny;
                    break;
                case EastGravity:
                case WestGravity:
                case CenterGravity:
                case StaticGravity:
                    ny += image->rows/2 - rows/2;
                    break;
                case NorthEastGravity:
                case NorthGravity:
                    // Don't let these run into the default case
                    break;
                default:
                    Data_Get_Struct(argv[0], MagickEnum, magick_enum);
                    rb_warning("gravity type `%s' has no effect", rb_id2name(magick_enum->id));
                    break;
            }

            x = ULONG2NUM(nx);
            y = ULONG2NUM(ny);
            break;
        case 4:
            x      = argv[0];
            y      = argv[1];
            width  = argv[2];
            height = argv[3];
            break;
        case 3:

            // Convert the width & height arguments to unsigned longs.
            // Compute the x & y offsets from the gravity and then
            // convert them to VALUEs.
            VALUE_TO_ENUM(argv[0], gravity, GravityType);
            width   = argv[1];
            height  = argv[2];
            columns = NUM2ULONG(width);
            rows    = NUM2ULONG(height);

            Data_Get_Struct(self, Image, image);

            switch (gravity)
            {
                case ForgetGravity:
                case NorthWestGravity:
                    nx = 0;
                    ny = 0;
                    break;
                case NorthGravity:
                    nx = (image->columns - columns) / 2;
                    ny = 0;
                    break;
                case NorthEastGravity:
                    nx = image->columns - columns;
                    ny = 0;
                    break;
                case WestGravity:
                    nx = 0;
                    ny = (image->rows - rows) / 2;
                    break;
                case EastGravity:
                    nx = image->columns - columns;
                    ny = (image->rows - rows) / 2;
                    break;
                case SouthWestGravity:
                    nx = 0;
                    ny = image->rows - rows;
                    break;
                case SouthGravity:
                    nx = (image->columns - columns) / 2;
                    ny = image->rows - rows;
                    break;
                case SouthEastGravity:
                    nx = image->columns - columns;
                    ny = image->rows - rows;
                    break;
                case StaticGravity:
                case CenterGravity:
                    nx = (image->columns - columns) / 2;
                    ny = (image->rows - rows) / 2;
                    break;
            }

            x = ULONG2NUM(nx);
            y = ULONG2NUM(ny);
            break;
        default:
            if (reset_page)
            {
                rb_raise(rb_eArgError, "wrong number of arguments (%d for 4, 5, or 6)", argc);
            }
            else
            {
                rb_raise(rb_eArgError, "wrong number of arguments (%d for 3, 4, or 5)", argc);
            }
            break;
    }

    cropped = xform_image(bang, self, x, y, width, height, CropImage);
    if (reset_page)
    {
        Data_Get_Struct(cropped, Image, image);
        image->page.x = image->page.y = 0L;
        image->page.width = image->page.height = 0UL;
    }
    return cropped;
}



/*
    Static:     xform_image
    Purpose:    call one of the image transformation functions
    Returns:    a new image, or transformed self
*/
static VALUE
xform_image(
    int bang,
    VALUE self,
    VALUE x,
    VALUE y,
    VALUE width,
    VALUE height,
    xformer_t xformer)
{
    Image *image, *new_image;
    RectangleInfo rect;
    ExceptionInfo exception;

    Data_Get_Struct(self, Image, image);
    rect.x      = NUM2LONG(x);
    rect.y      = NUM2LONG(y);
    rect.width  = NUM2ULONG(width);
    rect.height = NUM2ULONG(height);

    GetExceptionInfo(&exception);

    new_image = (xformer)(image, &rect, &exception);

    // An exception can occur in either the old or the new images
    rm_check_image_exception(image, RetainOnError);
    rm_check_exception(&exception, new_image, DestroyOnError);

    (void) DestroyExceptionInfo(&exception);

    rm_ensure_result(new_image);

    if (bang)
    {
        DATA_PTR(self) = new_image;
        (void) DestroyImage(image);
        return self;
    }

    return rm_image_new(new_image);

}


/*
    Extern:     extract_channels
    Purpose:    Remove all the ChannelType arguments from the
                end of the argument list.
    Returns:    A ChannelType value suitable for passing into
                an xMagick function. Returns AllChannels if
                no channel arguments were found. Returns the
                number of remaining arguments.
*/
ChannelType extract_channels(
    int *argc,
    VALUE *argv)
{
    volatile VALUE arg;
    ChannelType channels, ch_arg;

    channels = 0;
    while (*argc > 0)
    {
        arg = argv[(*argc)-1];

        // Stop when you find a non-ChannelType argument
        if (CLASS_OF(arg) != Class_ChannelType)
        {
            break;
        }
        VALUE_TO_ENUM(arg, ch_arg, ChannelType);
        channels |= ch_arg;
        *argc -= 1;
    }

    if (channels == 0)
    {
#if defined(HAVE_ALLCHANNELS)
        channels = AllChannels & ~OpacityChannel;
#else
        channels = 0xf7;
#endif
    }

    return channels;
}


/*
    Extern:     raise_ChannelType_error
    Purpose:    raise TypeError when an non-ChannelType object
                is unexpectedly encountered
*/
void
raise_ChannelType_error(VALUE arg)
{
    rb_raise(rb_eTypeError, "argument needs to be a ChannelType (%s given)"
            , rb_class2name(CLASS_OF(arg)));
}
