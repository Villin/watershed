/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    $RCSfile: itkSignedMaurerDistanceMapImageFilter.txx,v $
  Language:  C++
  Date:      $Date: 2008-01-19 19:50:01 $
  Version:   $Revision: 1.9 $

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __itkSignedMaurerDistanceMapImageFilter_txx
#define __itkSignedMaurerDistanceMapImageFilter_txx

#include "itkSignedMaurerDistanceMapImageFilter.h"
#include "itkImageRegionConstIteratorWithIndex.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkBinaryBorderImageFilter.h"
#include "itkProgressReporter.h"
#include "itkProgressAccumulator.h"
#include "vnl/vnl_vector.h"
#include "vnl/vnl_math.h"

//Simple functor to invert an image for Outside Danielsson distance map
namespace itk
{
namespace Functor
{
template <class InputPixelType> 
class InvertBinaryIntensityFunctor
{
public:
  InputPixelType operator()( InputPixelType input )
    {
    if( input )
      {
      return NumericTraits<InputPixelType>::Zero;
      }
    else
      {
      return NumericTraits<InputPixelType>::One;
      }
    }
};
}
}


namespace itk
{


template<class TInputImage, class TOutputImage>
SignedMaurerDistanceMapImageFilter<TInputImage, TOutputImage>
::SignedMaurerDistanceMapImageFilter() : m_BackgroundValue( 0 ),
                                         m_InsideIsPositive( false ),
                                         m_UseImageSpacing( false ),
                                         m_SquaredDistance( true )
{
}


template<class TInputImage, class TOutputImage>
SignedMaurerDistanceMapImageFilter<TInputImage, TOutputImage>
::~SignedMaurerDistanceMapImageFilter()
{
}


template<class TInputImage, class TOutputImage>
int
SignedMaurerDistanceMapImageFilter<TInputImage, TOutputImage>
::SplitRequestedRegion(int i, int num, OutputImageRegionType& splitRegion)
{
  // Get the output pointer
  OutputImageType * outputPtr = this->GetOutput();
  const typename TOutputImage::SizeType& requestedRegionSize 
    = outputPtr->GetRequestedRegion().GetSize();

  int splitAxis;
  typename TOutputImage::IndexType splitIndex;
  typename TOutputImage::SizeType splitSize;

  // Initialize the splitRegion to the output requested region
  splitRegion = outputPtr->GetRequestedRegion();
  splitIndex = splitRegion.GetIndex();
  splitSize = splitRegion.GetSize();

  // split on the outermost dimension available
  // and avoid the current dimension
  splitAxis = outputPtr->GetImageDimension() - 1;
  while (requestedRegionSize[splitAxis] == 1 || splitAxis == m_CurrentDimension)
    {
    --splitAxis;
    if (splitAxis < 0)
      { // cannot split
      itkDebugMacro("  Cannot Split");
      return 1;
      }
    }

  // determine the actual number of pieces that will be generated
  typename TOutputImage::SizeType::SizeValueType range = requestedRegionSize[splitAxis];
  int valuesPerThread = (int)::ceil(range/(double)num);
  int maxThreadIdUsed = (int)::ceil(range/(double)valuesPerThread) - 1;

  // Split the region
  if (i < maxThreadIdUsed)
    {
    splitIndex[splitAxis] += i*valuesPerThread;
    splitSize[splitAxis] = valuesPerThread;
    }
  if (i == maxThreadIdUsed)
    {
    splitIndex[splitAxis] += i*valuesPerThread;
    // last thread needs to process the "rest" dimension being split
    splitSize[splitAxis] = splitSize[splitAxis] - i*valuesPerThread;
    }
  
  // set the split region ivars
  splitRegion.SetIndex( splitIndex );
  splitRegion.SetSize( splitSize );

  itkDebugMacro("  Split Piece: " << splitRegion );

  return maxThreadIdUsed + 1;
}


template<class TInputImage, class TOutputImage>
void
SignedMaurerDistanceMapImageFilter<TInputImage, TOutputImage>
::GenerateData()
{
  // prepare the data
  this->AllocateOutputs();
  this->m_Spacing = this->GetOutput()->GetSpacing();

  typedef BinaryThresholdImageFilter<InputImageType, 
                                     OutputImageType
                                        > BinaryFilterType;
  
  ProgressAccumulator::Pointer progressAcc = ProgressAccumulator::New();
  progressAcc->SetMiniPipelineFilter(this);
  
  // compute the boundary of the binary object.
  // To do that, we erode the binary object. The eroded pixels are the ones
  // on the boundary. We mark them with the value 2
  typename BinaryFilterType::Pointer binaryFilter = BinaryFilterType::New();

  binaryFilter->SetLowerThreshold( this->m_BackgroundValue );
  binaryFilter->SetUpperThreshold( this->m_BackgroundValue );
  binaryFilter->SetInsideValue( NumericTraits< OutputPixelType >::max() );
  binaryFilter->SetOutsideValue( NumericTraits< OutputPixelType >::Zero );
  binaryFilter->SetInput( this->GetInput() );
  binaryFilter->SetNumberOfThreads( this->GetNumberOfThreads() );
  progressAcc->RegisterInternalFilter( binaryFilter, 0.1f );
  binaryFilter->GraftOutput( this->GetOutput() );
  binaryFilter->Update();

  // Dilate the inverted image by 1 pixel to give it the same boundary
  // as the univerted this->GetInput().
  typedef BinaryBorderImageFilter<OutputImageType, 
                                     OutputImageType
                                        > BorderFilterType;
  typename BorderFilterType::Pointer borderFilter = BorderFilterType::New();
  borderFilter->SetInput( binaryFilter->GetOutput() );
  borderFilter->SetForegroundValue( NumericTraits< OutputPixelType >::Zero );
  borderFilter->SetBackgroundValue( NumericTraits< OutputPixelType >::max() );
  borderFilter->SetFullyConnected(true);
  borderFilter->SetNumberOfThreads( this->GetNumberOfThreads() );
  progressAcc->RegisterInternalFilter( borderFilter, 0.23f );
  borderFilter->Update();
  this->GraftOutput( borderFilter->GetOutput() );
  
  // Set up the multithreaded processing
  typename ImageSource< TOutputImage >::ThreadStruct str;
  str.Filter = this;
  
  this->GetMultiThreader()->SetNumberOfThreads(this->GetNumberOfThreads());
  this->GetMultiThreader()->SetSingleMethod(this->ThreaderCallback, &str);
  
  // multithread the execution
  for( int d=0; d<ImageDimension; d++ )
    {
    m_CurrentDimension = d;
    this->GetMultiThreader()->SingleMethodExecute();
    }
}


template<class TInputImage, class TOutputImage>
void
SignedMaurerDistanceMapImageFilter<TInputImage, TOutputImage>
::ThreadedGenerateData(const OutputImageRegionType& outputRegionForThread, int threadId)
{

  vnl_vector<unsigned int> k(InputImageDimension-1);

  typedef typename InputImageType::RegionType   InputRegionType;

  InputRegionType region = outputRegionForThread;
  InputSizeType   size   = region.GetSize();
  typename InputImageType::RegionType::IndexType startIndex;
  startIndex = outputRegionForThread.GetIndex();

  // compute the number of rows first, so we can setup a progress reporter
  typename std::vector< unsigned int > NumberOfRows;
  unsigned long totalNumberOfRows = 0;

  for (unsigned int i = 0; i < InputImageDimension; i++)
    {
    NumberOfRows.push_back( 1 );
    for (unsigned int d = 0; d < InputImageDimension; d++)
      {
      if( d != i )
        {
        NumberOfRows[i] *= size[ d ];
        }
      }
    totalNumberOfRows += NumberOfRows[i];
    }

  // set the progress reporter. Use a pointer to be able to destroy it before the creation of progress2
  // so it won't set wrong progress at the end of ThreadedGenerateData()
  float progressPerDimension = 0.67f / ImageDimension;
  if( !this->m_SquaredDistance )
    {
    progressPerDimension = 0.67f / ( ImageDimension + 1 );
    }
  ProgressReporter * progress = new ProgressReporter(this, threadId, NumberOfRows[m_CurrentDimension], 30, 0.33f + m_CurrentDimension * progressPerDimension, progressPerDimension);

  unsigned int i = m_CurrentDimension;

    OutputIndexType idx;
    idx.Fill( 0 );

    k[0] = 1;
    unsigned int count = 1;


    for (unsigned int d = i+2; d < i+InputImageDimension; d++)
      {
      k[ count ] = k[ count-1 ] * size[ d % InputImageDimension ];
      count++;
      }
    k.flip();

    unsigned int index;
    for (unsigned int n = 0; n < NumberOfRows[i];n++)
      {
      index = n;
      count = 0;
      for (unsigned int d = i+1; d < i+InputImageDimension; d++)
        {
        idx[ d % InputImageDimension ] =
             static_cast<unsigned int>(
                 static_cast<double>( index ) /
                 static_cast<double>( k[count] ) )
             + startIndex[ d % InputImageDimension ];

        index %= k[ count ];
        count++;
        }
      this->Voronoi(i, idx);
      progress->CompletedPixel();
      }
  delete progress;

  if ( m_CurrentDimension == ImageDimension - 1 && !this->m_SquaredDistance )
    {
    typedef ImageRegionIterator< OutputImageType > OutputIterator;
    typedef ImageRegionConstIterator< InputImageType  > InputIterator;

    typename OutputImageType::RegionType outputRegion = outputRegionForThread;

    OutputIterator Ot( this->GetOutput(), outputRegion );
    InputIterator  It( this->GetInput(),  outputRegion );

    Ot.GoToBegin();
    It.GoToBegin();

    ProgressReporter progress2(this, threadId, outputRegionForThread.GetNumberOfPixels(), 30, 0.33f + ImageDimension * progressPerDimension, progressPerDimension);
    while( !Ot.IsAtEnd() )
      {
      
      // cast to a real type is required on some platforms 
      // TODO: use "typename NumericTraits<OutputPixelType>::RealType" instead
      // double. cableswig currently fail to build it with msvc 7.1
      const OutputPixelType outputValue =
        static_cast<OutputPixelType>(
          sqrt( static_cast<double>(vnl_math_abs( Ot.Get() ) ) ) );

      if( It.Get() != this->m_BackgroundValue )
        {
        if( this->GetInsideIsPositive() )
          {
          Ot.Set(  outputValue );
          }
        else
          {
          Ot.Set( -outputValue );
          }
        }
      else
        {
        if( this->GetInsideIsPositive() )
          {
          Ot.Set( -outputValue );
          }
        else
          {
          Ot.Set(  outputValue );
          }
        }

      ++Ot;
      ++It;
      progress2.CompletedPixel();
      }
    }

}


template<class TInputImage, class TOutputImage>
void
SignedMaurerDistanceMapImageFilter<TInputImage, TOutputImage>
::Voronoi(unsigned int d, OutputIndexType idx)
{
  typename OutputImageType::Pointer output(this->GetOutput());
  unsigned int nd = output->GetRequestedRegion().GetSize()[d];

  if( nd == 1 )
    {
    // nothing to do with that dimension
    return;
    }

  vnl_vector<OutputPixelType> g(nd);  g = 0;
  vnl_vector<OutputPixelType> h(nd);  h = 0;

  typename InputImageType::RegionType::IndexType startIndex;
  startIndex = this->GetInput()->GetRequestedRegion().GetIndex();

  OutputPixelType di;

  int l = -1;

  for( unsigned int i = 0; i < nd; i++ )
    {
    idx[d] = i + startIndex[d];

    di = output->GetPixel(idx);

    OutputPixelType iw;

    if( this->GetUseImageSpacing() )
      {
      iw = static_cast<OutputPixelType>( i * this->m_Spacing[d] );
      }
    else
      {
      iw  = static_cast<OutputPixelType>(i);
      }

    if( di != NumericTraits< OutputPixelType >::max() )
      {
      if( l < 1 )
        {
        l++;
        g(l) = di;
        h(l) = iw;
        }
      else
        {
        while( (l >= 1) &&
               this->Remove( g(l-1), g(l), di, h(l-1), h(l), iw) )
          {
          l--;
          }
        l++;
        g(l) = di;
        h(l) = iw;
        }
      }
    }

  if( l == -1 )
    {
    return;
    }

  int ns = l;

  l = 0;

  for( unsigned int i = 0; i < nd; i++ )
    {

    OutputPixelType iw;

    if( this->GetUseImageSpacing() )
      {
      iw = static_cast<OutputPixelType>( i * this->m_Spacing[d] );
      }
    else
      {
      iw = static_cast<OutputPixelType>( i );
      }

    OutputPixelType d1 = vnl_math_abs(g(l  )) + (h(l  )-iw)*(h(l  )-iw);

    while( l < ns )
      {
      // be sure to compute d2 *only* if l < ns
      OutputPixelType d2 = vnl_math_abs(g(l+1)) + (h(l+1)-iw)*(h(l+1)-iw);
      // then compare d1�and d2
      if( d1 <= d2 )
        break;
      l++;
      d1 = d2;
      }
    idx[d] = i + startIndex[d];

    if ( this->GetInput()->GetPixel( idx ) != this->m_BackgroundValue )
      {
      if ( this->m_InsideIsPositive )
        {
        output->SetPixel( idx,  d1 );
        }
      else
        {
        output->SetPixel( idx, -d1 );
        }
      }
    else
      {
      if ( this->m_InsideIsPositive )
        {
        output->SetPixel( idx, -d1 );
        }
      else
        {
        output->SetPixel( idx,  d1 );
        }
      }
    }
}


template<class TInputImage, class TOutputImage>
bool
SignedMaurerDistanceMapImageFilter<TInputImage, TOutputImage>
::Remove( OutputPixelType d1, OutputPixelType d2, OutputPixelType df,
          OutputPixelType x1, OutputPixelType x2, OutputPixelType xf )
{
  OutputPixelType a = x2 - x1;
  OutputPixelType b = xf - x2;
  OutputPixelType c = xf - x1;

  return ( (   c * vnl_math_abs( d2 ) - b * vnl_math_abs( d1 )
             - a * vnl_math_abs( df ) - a * b * c ) > 0);
}


/**
 * Standard "PrintSelf" method
 */
template <class TInputImage, class TOutputImage>
void
SignedMaurerDistanceMapImageFilter<TInputImage, TOutputImage>
::PrintSelf( std::ostream& os, Indent indent) const
{
  Superclass::PrintSelf( os, indent );
  os << indent << "Background Value: "
     << this->m_BackgroundValue << std::endl;
  os << indent << "Spacing: "
     << this->m_Spacing << std::endl;
  os << indent << "Inside is positive: "
     << this->m_InsideIsPositive << std::endl;
  os << indent << "Use image spacing: "
     << this->m_UseImageSpacing << std::endl;
  os << indent << "Squared distance: "
     << this->m_SquaredDistance << std::endl;
}

} // end namespace itk

#endif
