/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <math.h>
#include <string>
#include "ADM_default.h"
#include "ADM_coreVideoFilter.h"
#include "DIA_coreToolkit.h"
#include "DIA_factory.h"
#include "ADM_vidMisc.h"
#include "blend.h"
#include "blend_desc.cpp"
/**
        \class AVDM_BlendFrames
 *      \brief fade video plugin
 */
class AVDM_BlendFrames : public  ADM_coreVideoFilterCached
{
protected:
                blend         param;
                ADMImage     *buffer;
                bool         process(ADMImage *buffer,ADMImage *frame);
public:
                             AVDM_BlendFrames(ADM_coreVideoFilter *previous,CONFcouple *conf);
                             ~AVDM_BlendFrames();

        virtual const char   *getConfiguration(void);                   /// Return  current configuration as a human readable string
        virtual bool         getNextFrame(uint32_t *fn,ADMImage *image);    /// Return the next image
   //  virtual FilterInfo  *getInfo(void);                             /// Return picture parameters after this filter
        virtual bool         getCoupledConf(CONFcouple **couples) ;   /// Return the current filter configuration
        virtual void         setCoupledConf(CONFcouple *couples);
        virtual bool         configure(void) ;           /// Start graphical user interface

};

// Add the hook to make it valid plugin

DECLARE_VIDEO_FILTER(AVDM_BlendFrames,
                1,0,0,              // Version
                     ADM_UI_ALL,         // UI
                     VF_TRANSFORM,            // Category
                     "blend",            // internal name (must be uniq!)
                     QT_TRANSLATE_NOOP("blend","BlendFrames"),            // Display name
                     QT_TRANSLATE_NOOP("blend","Blend groups of N frames into a single frame.  Useful for speeding up slow motion footage or creating timelapses.") // Description
                 );   
/**
 * \fn configure
 * \brief UI configuration
 * @param 
 * @return 
 */
bool  AVDM_BlendFrames::configure()
{
    
  uint32_t N;

  diaElemUInteger N(&(param.N),QT_TRANSLATE_NOOP("blend","Frames"));
  diaElem *elems[1]={&N}
  if(diaFactoryRun(QT_TRANSLATE_NOOP("blend","Blend"),1,elems)){
    if(param.N<1)
      param.N=1;
    return 1;
  }else
    return 0;
}
/**
 *      \fn getConfiguration
 * 
 */
const char   *AVDM_BlendFrames::getConfiguration(void)
{
    static char conf[24];
    snprintf(conf,24," BlendFrames:%d ",param.N);
    return conf;
}

/**
 * \fn ctor
 * @param in
 * @param couples
 */
AVDM_BlendFrames::AVDM_BlendFrames(ADM_coreVideoFilter *in,CONFcouple *setup) :  ADM_coreVideoFilterCached(3,in,setup)//Q What does the 3 mean here?
{
    if(!setup || !ADM_paramLoad(setup,blend_param,&param))
    {
        // Default value
        param.N=1;
    }
    accumulated=0;
    buffer=NULL;
}
/**
 * \fn setCoupledConf
 * \brief save current setup from couples
 * @param couples
 */
void AVDM_BlendFrames::setCoupledConf(CONFcouple *couples)
{
    ADM_paramLoad(couples, blend_param, &param);
}

/**
 * \fn getCoupledConf
 * @param couples
 * @return setup as couples
 */
bool AVDM_BlendFrames::getCoupledConf(CONFcouple **couples)
{
    return ADM_paramSave(couples, blend_param,&param);
}

/**
 * \fn dtor
 */
AVDM_BlendFrames::~AVDM_BlendFrames(void)
{
    if(buffer)      
    {
      delete buffer;
      buffer=NULL;
    }
}
/**
 * 
 * @param source
 * @param source2
 * @param dest
 * @param offset
 * @return 
 */
bool AVDM_BlendFrames::AccumulateFrame(ADMImage *buffer,ADMImage *frame)
{
  
    uint8_t *bplanes[3]*;//Q uint32_t type image for accumulation?
    uint8_t *fplanes[3]*;
    int      bpitches[3],fpitches[3];

    buffer->GetWritePlanes(bplanes);
    buffer->GetPitches(bpitches);//Q Nice
    frame->GetReadPlanes(fplanes);
    frame->GetPitches(fpitches);

    for(int i=0;i<3;i++)
    {
        int    w=(int)frame->GetWidth((ADM_PLANE)i);
        int    h=(int)frame->GetHeight((ADM_PLANE)i);
        uint8_t *f=fplanes[i];
        uint8_t *b=bplanes[i];//Q uint32_t
        for(int y=0;y<h;y++)
        {
            for(int x=0;x<w;x++)
            {
                b[x]+=f[x];
            }
            b+=bpitches[i];
            f+=fpitches[i];
        }        
    }
    return true;
}

bool AVDM_BlendFrames::WriteFrameAndClearBuffer(ADMImage *buffer,ADMImage *frame,uint32_t N)
{
  
    uint8_t *bplanes[3]*;//Q uint32_t type image for accumulation?
    uint8_t *fplanes[3]*;
    int      bpitches[3],fpitches[3];

    buffer->GetWritePlanes(bplanes);
    buffer->GetPitches(bpitches);//Q Nice
    frame->GetReadPlanes(fplanes);
    frame->GetPitches(fpitches);

    for(int i=0;i<3;i++)
    {
        int    w=(int)frame->GetWidth((ADM_PLANE)i);
        int    h=(int)frame->GetHeight((ADM_PLANE)i);
        uint8_t *f=fplanes[i];
        uint8_t *b=bplanes[i];//Q uint32_t
        for(int y=0;y<h;y++)
        {
            for(int x=0;x<w;x++)
            {
                f[x]=(uint8_t)b[x]/N;//The meat of the matter...
                b[x]=0;//Clear buffer here as well because why not
            }
            b+=bpitches[i];
            f+=fpitches[i];
        }        
    }
    return true;
}

/**
 * \fn getNextFrame
 * @param fn
 * @param image
 * @return 
 */
bool AVDM_BlendFrames::getNextFrame(uint32_t *fn,ADMImage *image)
{
  ADMImage *frame=vidCache->getImage(fn);
  //Create new buffer
  if(buffer==NULL)
    buffer=new ADMImageDefault(frame->GetWidth(PLANAR_Y),frame->GetHeight(PLANAR_Y));
  AccumulateFrame(buffer,frame);
  accumulated++;
  if(accumulated==param.N){
    accumulated=0;
    //Divide buffer by N and write to 'image'
    //Reset buffer to 0
    WriteFrameAndClearBuffer(buffer,frame,param.N);
    return true;
  }
  return false;
}
//EOF