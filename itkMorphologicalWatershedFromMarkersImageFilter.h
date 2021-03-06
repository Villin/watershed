#ifndef __itkMorphologicalWatershedFromMarkersImageFilter_h
#define __itkMorphologicalWatershedFromMarkersImageFilter_h

#include "itkImageToImageFilter.h"
#include "itkConnectivity.h"

namespace itk {

/** \class MorphologicalWatershedFromMarkersImageFilter
 * \brief Morphological watershed transform from markers
 *
 * TODO
 * 
 * Watershed pixel are labeled 0.
 * TOutputImage should be an integer type.
 * The marker image must contain labeled component (each component
 * have a different value).
 * Labels of output image are the label of the marker image.
 *
 * The morphological watershed transform algorithm is described in
 * Chapter 9.2 of Pierre Soille's book "Morphological Image Analysis:
 * Principles and Applications", Second Edition, Springer, 2003.
 *
 * \author Ga�tan Lehmann. Biologie du D�veloppement et de la Reproduction, INRA de Jouy-en-Josas, France.
 *
 * \sa WatershedImageFilter, MorphologicalWatershedImageFilter
 * \ingroup ImageEnhancement  MathematicalMorphologyImageFilters
 */
template<class TInputImage, class TLabelImage>
class ITK_EXPORT MorphologicalWatershedFromMarkersImageFilter : 
    public ImageToImageFilter<TInputImage, TLabelImage>
{
public:
  /** Standard class typedefs. */
  typedef MorphologicalWatershedFromMarkersImageFilter Self;
  typedef ImageToImageFilter<TInputImage, TLabelImage>
  Superclass;
  typedef SmartPointer<Self>        Pointer;
  typedef SmartPointer<const Self>  ConstPointer;

  /** Some convenient typedefs. */
  typedef TInputImage InputImageType;
  typedef TLabelImage LabelImageType;
  typedef typename InputImageType::Pointer        InputImagePointer;
  typedef typename InputImageType::ConstPointer   InputImageConstPointer;
  typedef typename InputImageType::RegionType     InputImageRegionType;
  typedef typename InputImageType::PixelType      InputImagePixelType;
  typedef typename LabelImageType::Pointer        LabelImagePointer;
  typedef typename LabelImageType::ConstPointer   LabelImageConstPointer;
  typedef typename LabelImageType::RegionType     LabelImageRegionType;
  typedef typename LabelImageType::PixelType      LabelImagePixelType;
  
  typedef typename LabelImageType::IndexType      IndexType;

  /** ImageDimension constants */
  itkStaticConstMacro(ImageDimension, unsigned int,
                      TInputImage::ImageDimension);

  typedef Connectivity< ImageDimension > ConnectivityType;
  
  /** Standard New method. */
  itkNewMacro(Self);  

  /** Runtime information support. */
  itkTypeMacro(MorphologicalWatershedFromMarkersImageFilter, 
               ImageToImageFilter);
  

   /** Set the marker image */
  void SetMarkerImage(TLabelImage *input)
     {
     // Process object is not const-correct so the const casting is required.
     this->SetNthInput( 1, const_cast<TLabelImage *>(input) );
     }

  /** Get the marker image */
  LabelImageType * GetMarkerImage()
    {
    return static_cast<LabelImageType*>(const_cast<DataObject *>(this->ProcessObject::GetInput(1)));
    }

   /** Set the input image */
  void SetInput1(TInputImage *input)
     {
     this->SetInput( input );
     }

   /** Set the marker image */
  void SetInput2(TLabelImage *input)
     {
     this->SetMarkerImage( input );
     }

  /**
   * Set/Get whether the connected components are defined strictly by
   * face connectivity or by face+edge+vertex connectivity.  Default is
   * FullyConnectedOff.  For objects that are 1 pixel wide, use
   * FullyConnectedOn.
   */
  void SetFullyConnected( bool value )
    {
    int oldCellDimension = m_Connectivity->GetCellDimension();
    m_Connectivity->SetFullyConnected( value );
    if( oldCellDimension != m_Connectivity->GetCellDimension() )
      {
      this->Modified();
      }
    }

  bool GetFullyConnected() const
    {
    return m_Connectivity->GetFullyConnected();
    }

  void FullyConnectedOn()
    {
    this->SetFullyConnected( true );
    }

  void FullyConnectedOff()
    {
    this->SetFullyConnected( false );
    }


  /**
   * Set/Get whether the watershed pixel must be marked or not. Default
   * is true. Set it to false do not only avoid writing watershed pixels,
   * it also decrease algorithm complexity.
   */
  itkSetMacro(MarkWatershedLine, bool);
  itkGetConstReferenceMacro(MarkWatershedLine, bool);
  itkBooleanMacro(MarkWatershedLine);

  itkSetMacro(UseImageSpacing, bool);
  itkGetConstReferenceMacro(UseImageSpacing, bool);
  itkBooleanMacro(UseImageSpacing);

  /**
   * Get/Set the connectivity to be use by the watershed filter.
   */
  itkSetObjectMacro( Connectivity, ConnectivityType );
  itkGetObjectMacro( Connectivity, ConnectivityType );
  itkGetConstObjectMacro( Connectivity, ConnectivityType );

  /**
   */
  itkSetMacro(BackgroundValue, LabelImagePixelType);
  itkGetMacro(BackgroundValue, LabelImagePixelType);

protected:
  MorphologicalWatershedFromMarkersImageFilter();
  ~MorphologicalWatershedFromMarkersImageFilter() {};
  void PrintSelf(std::ostream& os, Indent indent) const;

  /** MorphologicalWatershedFromMarkersImageFilter needs to request enough of the
   * marker image to account for the elementary structuring element.
   * The mask image does not need to be padded. Depending on whether
   * the filter is configured to run a single iteration or until
   * convergence, this method may request all of the marker and mask
   * image be provided. */
  void GenerateInputRequestedRegion();

  /** This filter will enlarge the output requested region to produce
   * all of the output if the filter is configured to run to
   * convergence.
   * \sa ProcessObject::EnlargeOutputRequestedRegion() */
  void EnlargeOutputRequestedRegion(DataObject *itkNotUsed(output));

  /** Single-threaded version of GenerateData.  This version is used
   * when the filter is configured to run to convergence. This method
   * may delegate to the multithreaded version if the filter is
   * configured to run a single iteration.  Otherwise, it will
   * delegate to a separate instance to run each iteration until the
   * filter converges. */
  void GenerateData();
  
private:
  MorphologicalWatershedFromMarkersImageFilter(const Self&); //purposely not implemented
  void operator=(const Self&); //purposely not implemented

  typename ConnectivityType::Pointer m_Connectivity;

  bool m_MarkWatershedLine;
  bool m_PadImageBoundary;
  bool m_UseImageSpacing;
  LabelImagePixelType m_BackgroundValue;

  typedef Image< float, ImageDimension > DistanceImageType;
  typedef std::vector<typename DistanceImageType::PixelType> WeightType;
  typedef typename DistanceImageType::PixelType DistancePixelType;

} ; // end of class

} // end namespace itk
  
#ifndef ITK_MANUAL_INSTANTIATION
#include "itkMorphologicalWatershedFromMarkersImageFilter.txx"
#endif

#endif


