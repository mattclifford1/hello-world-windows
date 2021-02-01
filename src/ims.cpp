/*
Software License :

Copyright (c) 2014, The Open Effects Association Ltd. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name The Open Effects Association Ltd, nor the names of its
      contributors may be used to endorse or promote products derived from this
      software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
  Author : Bruno Nicoletti (2014)

  This plugin will take you through the basics of defining and using
  parameters as well as how to use instance data.

  The accompanying guide will explain what is happening in more detail.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <string>
#include <iostream>

// the one OFX header we need, it includes the others necessary
#include "ofxImageEffect.h"
#include "libim.h"

#if defined __APPLE__ || defined linux
#  define EXPORT __attribute__((visibility("default")))
#elif defined _WIN32
#  define EXPORT OfxExport
#else
#  error Not building on your operating system quite yet
#endif

////////////////////////////////////////////////////////////////////////////////
// macro to write a labelled message to stderr with
#ifdef _WIN32
  #define DUMP(LABEL, MSG, ...)                                           \
  {                                                                       \
    fprintf(stderr, "%s%s:%d in %s ", LABEL, __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, MSG, ##__VA_ARGS__);                                  \
    fprintf(stderr, "\n");                                                \
  }
#else
  #define DUMP(LABEL, MSG, ...)                                           \
  {                                                                       \
    fprintf(stderr, "%s%s:%d in %s ", LABEL, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    fprintf(stderr, MSG, ##__VA_ARGS__);                                  \
    fprintf(stderr, "\n");                                                \
  }
#endif

// macro to write a simple message, only works if 'VERBOSE' is #defined
//#define VERBOSE
#ifdef VERBOSE
#  define MESSAGE(MSG, ...) DUMP("", MSG, ##__VA_ARGS__)
#else
#  define MESSAGE(MSG, ...)
#endif

// macro to dump errors to stderr if the given condition is true
#define ERROR_IF(CONDITION, MSG, ...) if(CONDITION) { DUMP("ERROR : ", MSG, ##__VA_ARGS__);}

// macro to dump errors to stderr and abort if the given condition is true
#define ERROR_ABORT_IF(CONDITION, MSG, ...)     \
{                                               \
  if(CONDITION) {                               \
    DUMP("FATAL ERROR : ", MSG, ##__VA_ARGS__); \
    abort();                                    \
  }                                             \
}

// name of our two params
#define SATURATION_PARAM_NAME_R "R Channel"
#define SATURATION_PARAM_NAME_G "G Channel"
#define SATURATION_PARAM_NAME_B "B Channel"

// anonymous namespace to hide our symbols in
namespace {
  ////////////////////////////////////////////////////////////////////////////////
  // set of suite pointers provided by the host
  OfxHost               *gHost;
  OfxPropertySuiteV1    *gPropertySuite    = 0;
  OfxImageEffectSuiteV1 *gImageEffectSuite = 0;
  OfxParameterSuiteV1   *gParameterSuite   = 0;

  ////////////////////////////////////////////////////////////////////////////////
  // class to manage OFX images
  class Image {
  public    :
    // construct from a property set that represents the image
    Image(OfxPropertySetHandle propSet);

    // construct from a clip by fetching an image at the given frame
    Image(OfxImageClipHandle clip, double frame);

    // destructor
    ~Image();

    // get a pixel address, cast to the right type
    template <class T>
    T *pixelAddress(int x, int y)
    {
      return reinterpret_cast<T *>(rawAddress(x, y));
    }

    // Is this image empty?
    operator bool();

    // bytes per component, 1, 2 or 4 for byte, short and float images
    int bytesPerComponent() const { return bytesPerComponent_; }

    // number of components
    int nComponents() const { return nComponents_; }

  protected :
    void construct();

    // Look up a pixel address in the image. returns null if the pixel was not
    // in the bounds of the image
    void *rawAddress(int x, int y);

    OfxPropertySetHandle propSet_;
    int rowBytes_;
    OfxRectI bounds_;
    char *dataPtr_;
    int nComponents_;
    int bytesPerComponent_;
    int bytesPerPixel_;
  };

  // construct from a property set
  Image::Image(OfxPropertySetHandle propSet)
    : propSet_(propSet)
  {
    construct();
  }

  // construct by fetching from a clip
  Image::Image(OfxImageClipHandle clip, double time)
    : propSet_(NULL)
  {
    if (clip && (gImageEffectSuite->clipGetImage(clip, time, NULL, &propSet_) == kOfxStatOK)) {
      construct();
    }
    else {
      propSet_ = NULL;
    }
  }

  // assemble it all togther
  void Image::construct()
  {
    if(propSet_) {
      gPropertySuite->propGetInt(propSet_, kOfxImagePropRowBytes, 0, &rowBytes_);
      gPropertySuite->propGetIntN(propSet_, kOfxImagePropBounds, 4, &bounds_.x1);
      gPropertySuite->propGetPointer(propSet_, kOfxImagePropData, 0, (void **) &dataPtr_);

      // how many components per pixel?
      char *cstr;
      gPropertySuite->propGetString(propSet_, kOfxImageEffectPropComponents, 0, &cstr);

      if(strcmp(cstr, kOfxImageComponentRGBA) == 0) {
        nComponents_ = 4;
      }
      else if(strcmp(cstr, kOfxImageComponentRGB) == 0) {
        nComponents_ = 3;
      }
      else if(strcmp(cstr, kOfxImageComponentAlpha) == 0) {
        nComponents_ = 1;
      }
      else {
        throw " bad pixel type!";
      }

      // what is the data type
      gPropertySuite->propGetString(propSet_, kOfxImageEffectPropPixelDepth, 0, &cstr);
      if(strcmp(cstr, kOfxBitDepthByte) == 0) {
        bytesPerComponent_ = 1;
      }
      else if(strcmp(cstr, kOfxBitDepthShort) == 0) {
        bytesPerComponent_ = 2;
      }
      else if(strcmp(cstr, kOfxBitDepthFloat) == 0) {
        bytesPerComponent_ = 4;
      }
      else {
        throw " bad pixel type!";
      }

      bytesPerPixel_ = bytesPerComponent_ * nComponents_;
    }
    else {
      rowBytes_ = 0;
      bounds_.x1 = bounds_.x2 = bounds_.y1 = bounds_.y2 = 0;
      dataPtr_ = NULL;
      nComponents_ = 0;
      bytesPerComponent_ = 0;
    }
  }

  // destructor
  Image::~Image()
  {
    if(propSet_)
      gImageEffectSuite->clipReleaseImage(propSet_);
  }

  // get the address of a location in the image as a void *
  void *Image::rawAddress(int x, int y)
  {
    // Inside the bounds of this image?
    if(x < bounds_.x1 || x >= bounds_.x2 || y < bounds_.y1 || y >= bounds_.y2)
      return NULL;

    // turn image plane coordinates into offsets from the bottom left
    int yOffset = y - bounds_.y1;
    int xOffset = x - bounds_.x1;

    // Find the start of our row, using byte arithmetic
    char *rowStart = (dataPtr_) + yOffset * rowBytes_;

    // finally find the position of the first component of column
    return rowStart + (xOffset * bytesPerPixel_);
  }

  // are we empty?
  Image:: operator bool()
  {
    return propSet_ != NULL && dataPtr_ != NULL;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // our instance data, where we are caching away clip and param handles
  struct MyInstanceData {
    // are we in the general context
    bool isGeneralContext;

    // handles to the clips we deal with
    OfxImageClipHandle sourceClip;
    OfxImageClipHandle maskClip;
    OfxImageClipHandle outputClip;

    // handles to a our parameters
    OfxParamHandle saturationParamR;
    OfxParamHandle saturationParamG;
    OfxParamHandle saturationParamB;

    MyInstanceData()
      : isGeneralContext(false)
      , sourceClip(NULL)
      , maskClip(NULL)
      , outputClip(NULL)
      , saturationParamR(NULL)
      , saturationParamG(NULL)
      , saturationParamB(NULL)
    {}
  };

  ////////////////////////////////////////////////////////////////////////////////
  // get my instance data from a property set handle
  MyInstanceData *FetchInstanceData(OfxPropertySetHandle effectProps)
  {
    MyInstanceData *myData = 0;
    gPropertySuite->propGetPointer(effectProps,
                                   kOfxPropInstanceData,
                                   0,
                                   (void **) &myData);
    return myData;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // get my instance data
  MyInstanceData *FetchInstanceData(OfxImageEffectHandle effect)
  {
    // get the property handle for the plugin
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(effect, &effectProps);

    // and get the instance data out of that
    return FetchInstanceData(effectProps);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // get the named suite and put it in the given pointer, with error checking
  template <class SUITE>
  void FetchSuite(SUITE *& suite, const char *suiteName, int suiteVersion)
  {
    suite = (SUITE *) gHost->fetchSuite(gHost->host, suiteName, suiteVersion);
    if(!suite) {
      ERROR_ABORT_IF(suite == NULL,
                     "Failed to fetch %s verison %d from the host.",
                     suiteName,
                     suiteVersion);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  // The first _action_ called after the binary is loaded (three boot strapper functions will be howeever)
  OfxStatus LoadAction(void)
  {
    // fetch our three suites
    FetchSuite(gPropertySuite,    kOfxPropertySuite,    1);
    FetchSuite(gImageEffectSuite, kOfxImageEffectSuite, 1);
    FetchSuite(gParameterSuite,   kOfxParameterSuite,   1);

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // the plugin's basic description routine
  OfxStatus DescribeAction(OfxImageEffectHandle descriptor)
  {
    // get the property set handle for the plugin
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(descriptor, &effectProps);

    // set some labels and the group it belongs to
    gPropertySuite->propSetString(effectProps,
                                  kOfxPropLabel,
                                  0,
                                  "Adjust RGB");
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPluginPropGrouping,
                                  0,
                                  "Hello World!");

    // define the image effects contexts we can be used in, in this case a filter
    // and a general effect
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedContexts,
                                  0,
                                  kOfxImageEffectContextFilter);

    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedContexts,
                                  1,
                                  kOfxImageEffectContextGeneral);

    // set the bit depths the plugin can handle
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedPixelDepths,
                                  0,
                                  kOfxBitDepthFloat);
    // gPropertySuite->propSetString(effectProps,
    //                               kOfxImageEffectPropSupportedPixelDepths,
    //                               1,
    //                               kOfxBitDepthShort);
    // gPropertySuite->propSetString(effectProps,
    //                               kOfxImageEffectPropSupportedPixelDepths,
    //                               2,
    //                               kOfxBitDepthByte);

    // say that a single instance of this plugin canNOT be rendered in multiple threads
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPluginRenderThreadSafety,
                                  0,
                                  kOfxImageEffectRenderUnsafe);

    // say that the host should NOT manage SMP threading over a single frame
    gPropertySuite->propSetInt(effectProps,
                               kOfxImageEffectPluginPropHostFrameThreading,
                               0,
                               0);
    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  //  describe the plugin in context
  OfxStatus
  DescribeInContextAction(OfxImageEffectHandle descriptor,
                          OfxPropertySetHandle inArgs)
  {
    // get the context we are being described for
    char *context;
    gPropertySuite->propGetString(inArgs, kOfxImageEffectPropContext, 0, &context);

    OfxPropertySetHandle props;
    // define the mandated single output clip
    gImageEffectSuite->clipDefine(descriptor, "Output", &props);

    // set the component types we can handle on out output
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  0,
                                  kOfxImageComponentRGBA);
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  1,
                                  kOfxImageComponentRGB);

    // define the mandated single source clip
    gImageEffectSuite->clipDefine(descriptor, "Source", &props);

    // set the component types we can handle on our main input
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  0,
                                  kOfxImageComponentRGBA);
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  1,
                                  kOfxImageComponentRGB);


    // first get the handle to the parameter set
    OfxParamSetHandle paramSet;
    gImageEffectSuite->getParamSet(descriptor, &paramSet);

    // properties on our parameter RED
    OfxPropertySetHandle paramPropsR;

    // now define a 'saturation' parameter and set its properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 SATURATION_PARAM_NAME_R,
                                 &paramPropsR);
    gPropertySuite->propSetString(paramPropsR,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramPropsR,
                                  kOfxParamPropDefault,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramPropsR,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  -1.0);
    gPropertySuite->propSetDouble(paramPropsR,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  1.0);
    gPropertySuite->propSetString(paramPropsR,
                                  kOfxPropLabel,
                                  0,
                                  "RED");
    gPropertySuite->propSetString(paramPropsR,
                                  kOfxParamPropHint,
                                  0,
                                  "Adjust RED Channel");


    // properties on our parameter GREEN
    OfxPropertySetHandle paramPropsG;

    // now define a 'saturation' parameter and set its properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 SATURATION_PARAM_NAME_G,
                                 &paramPropsG);
    gPropertySuite->propSetString(paramPropsG,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramPropsG,
                                  kOfxParamPropDefault,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramPropsG,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  -1.0);
    gPropertySuite->propSetDouble(paramPropsG,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  1.0);
    gPropertySuite->propSetString(paramPropsG,
                                  kOfxPropLabel,
                                  0,
                                  "GREEN");
    gPropertySuite->propSetString(paramPropsG,
                                  kOfxParamPropHint,
                                  0,
                                  "Adjust GREEN Channel");


    // properties on our parameter BLUE
    OfxPropertySetHandle paramPropsB;

    // now define a 'saturation' parameter and set its properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 SATURATION_PARAM_NAME_B,
                                 &paramPropsB);
    gPropertySuite->propSetString(paramPropsB,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramPropsB,
                                  kOfxParamPropDefault,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramPropsB,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  -1.0);
    gPropertySuite->propSetDouble(paramPropsB,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  1.0);
    gPropertySuite->propSetString(paramPropsB,
                                  kOfxPropLabel,
                                  0,
                                  "BLUE");
    gPropertySuite->propSetString(paramPropsB,
                                  kOfxParamPropHint,
                                  0,
                                  "Adjust BLUE Channel");

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// instance construction
  OfxStatus CreateInstanceAction( OfxImageEffectHandle instance)
  {
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(instance, &effectProps);

    // To avoid continual lookup, put our handles into our instance
    // data, those handles are guaranteed to be valid for the duration
    // of the instance.
    MyInstanceData *myData = new MyInstanceData;

    // Set my private instance data
    gPropertySuite->propSetPointer(effectProps, kOfxPropInstanceData, 0, (void *) myData);

    // is this instance made for the general context?
    char *context = 0;
    gPropertySuite->propGetString(effectProps, kOfxImageEffectPropContext, 0,  &context);
    myData->isGeneralContext = context && (strcmp(context, kOfxImageEffectContextGeneral) == 0);

    // Cache the source and output clip handles
    gImageEffectSuite->clipGetHandle(instance, "Source", &myData->sourceClip, 0);
    gImageEffectSuite->clipGetHandle(instance, "Output", &myData->outputClip, 0);

    // Cache away the param handles
    OfxParamSetHandle paramSet;
    gImageEffectSuite->getParamSet(instance, &paramSet);
    gParameterSuite->paramGetHandle(paramSet,
                                    SATURATION_PARAM_NAME_R,
                                    &myData->saturationParamR,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    SATURATION_PARAM_NAME_G,
                                    &myData->saturationParamG,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    SATURATION_PARAM_NAME_B,
                                    &myData->saturationParamB,
                                    0);

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // instance destruction
  OfxStatus DestroyInstanceAction( OfxImageEffectHandle instance)
  {
    // get my instance data
    MyInstanceData *myData = FetchInstanceData(instance);
    delete myData;

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // iterate over our pixels and process them
  template <class T, int MAX>
  void PixelProcessing(double params[3],
                       OfxImageEffectHandle instance,
                       Image &src,
                       Image &output,
                       OfxRectI renderWindow)
  {
    // see how many channels
    int nComps = output.nComponents();
    std::cout<<"Render Window Cols: "<<renderWindow.y2-renderWindow.y1<<"\n";
    std::cout<<"Render Window Rows: "<<renderWindow.x2-renderWindow.x1<<"\n";
    std::cout<<"Params: "<<params[0]<<" "<<params[1]<<" "<<params[2]<<"\n\n";

    // get image copier initalised
    int rows = renderWindow.y2 - renderWindow.y1;
    int cols = renderWindow.x2 - renderWindow.x1;
    imageCopy im(rows, cols, nComps);


    // copy source image
    for(int row = renderWindow.y1; row < renderWindow.y2; row++) {
      for(int col = renderWindow.x1; col < renderWindow.x2; col++) {
        T *srcPix = src.pixelAddress<T>(col, row);
        im.initCopy(srcPix[0]);
        im.initCopy(srcPix[1]);
        im.initCopy(srcPix[2]);
        if(nComps == 4){
          im.initCopy(srcPix[3]);
        }
      }
    }

    // and do some processing
    for(int row = 0; row < rows; row++) {
      for(int col = 0; col < cols; col++) {
        im.setPix(row, col, 0, im.getPix(row, col, 0)+params[0]);
        im.setPix(row, col, 1, im.getPix(row, col, 1)+params[1]);
        im.setPix(row, col, 2, im.getPix(row, col, 2)+params[2]);
      }
    }
    // im.toTensor();   // convert to pytorch tensor
    // im.getTensor() += saturation;

    // copy back to output image
    int rowCounter;
    int colCounter;
    rowCounter = 0;
    for(int row = renderWindow.y1; row < renderWindow.y2; row++) {
      // get start of output image memory row
      T *dstPix = output.pixelAddress<T>(renderWindow.x1, row);
      colCounter = 0;
      for(int col = renderWindow.x1; col < renderWindow.x2; col++) {
        // apply to output
        dstPix[0] = im.getPix(rowCounter, colCounter, 0);
        dstPix[1] = im.getPix(rowCounter, colCounter, 1);
        dstPix[2] = im.getPix(rowCounter, colCounter, 2);
        if(nComps == 4) { // if we have an alpha
          dstPix[3] = im.getPix(rowCounter, colCounter, 3);
        }
        dstPix += nComps;
        colCounter++;
      }
      rowCounter++;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Render an output image
  OfxStatus RenderAction( OfxImageEffectHandle instance,
                          OfxPropertySetHandle inArgs,
                          OfxPropertySetHandle outArgs)
  {
    // get the render window and the time from the inArgs
    OfxTime time;
    OfxRectI renderWindow;
    OfxStatus status = kOfxStatOK;

    gPropertySuite->propGetDouble(inArgs,
                                  kOfxPropTime,
                                  0,
                                  &time);
    gPropertySuite->propGetIntN(inArgs,
                                kOfxImageEffectPropRenderWindow,
                                4,
                                &renderWindow.x1);

    // get our instance data which has out clip and param handles
    MyInstanceData *myData = FetchInstanceData(instance);

    // get our param values
    double params [3] = {0.0, 0.0, 0.0};
    gParameterSuite->paramGetValueAtTime(myData->saturationParamR, time, &params[0]);
    gParameterSuite->paramGetValueAtTime(myData->saturationParamG, time, &params[1]);
    gParameterSuite->paramGetValueAtTime(myData->saturationParamB, time, &params[2]);

    // the property sets holding our images
    OfxPropertySetHandle outputImg = NULL, sourceImg = NULL;
    try {
      // fetch image to render into from that clip
      Image outputImg(myData->outputClip, time);
      if(!outputImg) {
        throw " no output image!";
      }

      // fetch image to render into from that clip
      Image sourceImg(myData->sourceClip, time);
      if(!sourceImg) {
        throw " no source image!";
      }

      // now do our render depending on the data type
      if(outputImg.bytesPerComponent() == 4) {
        PixelProcessing<float, 1>(params,
          instance,
          sourceImg,
          outputImg,
          renderWindow);
        }
      else {
        throw " bad data type!";
        throw 1;
      }

    }
    catch(const char *errStr ) {
      bool isAborting = gImageEffectSuite->abort(instance);

      // if we were interrupted, the failed fetch is fine, just return kOfxStatOK
      // otherwise, something wierd happened
      if(!isAborting) {
        status = kOfxStatFailed;
      }
      ERROR_IF(!isAborting, " Rendering failed because %s", errStr);
    }

    // all was well
    return status;
  }

  // are the settings of the effect making it redundant and so not do anything to the image data
  OfxStatus IsIdentityAction( OfxImageEffectHandle instance,
                              OfxPropertySetHandle inArgs,
                              OfxPropertySetHandle outArgs)
  {
    MyInstanceData *myData = FetchInstanceData(instance);

    double time;
    gPropertySuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

    // get our param values
    double params [3] = {0.0, 0.0, 0.0};
    gParameterSuite->paramGetValueAtTime(myData->saturationParamR, time, &params[0]);
    gParameterSuite->paramGetValueAtTime(myData->saturationParamG, time, &params[1]);
    gParameterSuite->paramGetValueAtTime(myData->saturationParamB, time, &params[2]);

    // if all the params values are 0 (or nearly so) say we aren't doing anything
    if(fabs(params[0]) < 0.000000001 && fabs(params[1]) < 0.000000001 && fabs(params[2]) < 0.000000001) {
      // we set the name of the input clip to pull default images from
      gPropertySuite->propSetString(outArgs, kOfxPropName, 0, "Source");
      // and say we trapped the action and we are at the identity
      return kOfxStatOK;
    }

    // say we aren't at the identity
    return kOfxStatReplyDefault;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // The main entry point function, the host calls this to get the plugin to do things.
  OfxStatus MainEntryPoint(const char *action, const void *handle, OfxPropertySetHandle inArgs,  OfxPropertySetHandle outArgs)
  {
    MESSAGE(": START action is : %s \n", action );
    // cast to appropriate type
    OfxImageEffectHandle effect = (OfxImageEffectHandle) handle;

    OfxStatus returnStatus = kOfxStatReplyDefault;

    if(strcmp(action, kOfxActionLoad) == 0) {
      // The very first action called on a plugin.
      returnStatus = LoadAction();
    }
    else if(strcmp(action, kOfxActionDescribe) == 0) {
      // the first action called to describe what the plugin does
      returnStatus = DescribeAction(effect);
    }
    else if(strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
      // the second action called to describe what the plugin does in a specific context
      returnStatus = DescribeInContextAction(effect, inArgs);
    }
    else if(strcmp(action, kOfxActionCreateInstance) == 0) {
      // the action called when an instance of a plugin is created
      returnStatus = CreateInstanceAction(effect);
    }
    else if(strcmp(action, kOfxActionDestroyInstance) == 0) {
      // the action called when an instance of a plugin is destroyed
      returnStatus = DestroyInstanceAction(effect);
    }
    else if(strcmp(action, kOfxImageEffectActionIsIdentity) == 0) {
      // Check to see if our param settings cause nothing to happen
      returnStatus = IsIdentityAction(effect, inArgs, outArgs);
    }
    else if(strcmp(action, kOfxImageEffectActionRender) == 0) {
      // action called to render a frame
      returnStatus = RenderAction(effect, inArgs, outArgs);
    }

    MESSAGE(": END action is : %s \n", action );
    /// other actions to take the default value
    return returnStatus;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Call back passed to the host in the OfxPlugin struct to set our host pointer
  //
  // This must be called AFTER both OfxGetNumberOfPlugins and OfxGetPlugin, but
  // before the pluginMain entry point is ever touched.
  void SetHostFunc(OfxHost *hostStruct)
  {
    gHost = hostStruct;
  }

} // end of anonymous namespace


////////////////////////////////////////////////////////////////////////////////
// The plugin struct passed back to the host application to initiate bootstrapping\
// of plugin communications
static OfxPlugin effectPluginStruct =
{
  kOfxImageEffectPluginApi,                  // The API this plugin satisfies.
  1,                                         // The version of the API it satisifes.
  "hello.world.nulight.example", // The unique ID of this plugin.
  1,                                         // The major version number of this plugin.
  0,                                         // The minor version number of this plugin.
  SetHostFunc,                               // Function used to pass back to the plugin the OFXHost struct.
  MainEntryPoint                             // The main entry point to the plugin where all actions are passed to.
};

////////////////////////////////////////////////////////////////////////////////
// The first of the two functions that a host application will look for
// after loading the binary, this function returns the number of plugins within
// this binary.
//
// This will be the first function called by the host.
EXPORT int OfxGetNumberOfPlugins(void)
{
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// The second of the two functions that a host application will look for
// after loading the binary, this function returns the 'nth' plugin declared in
// this binary.
//
// This will be called multiple times by the host, once for each plugin present.
EXPORT OfxPlugin * OfxGetPlugin(int nth)
{
  if(nth == 0)
    return &effectPluginStruct;
  return 0;
}
