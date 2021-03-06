#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkCommand.h"
#include "itkNumericTraits.h"
#include <itkIntensityWindowingImageFilter.h>
#include <itkMinimumMaximumImageCalculator.h>
#include "itkMorphologicalWatershedImageFilter.h"
#include "itkLabelOverlayImageFilter.h"
#include "itkChangeInformationImageFilter.h"

#include "itkGrayscaleDilateImageFilter.h"
#include "itkGrayscaleErodeImageFilter.h"


#include "itkBinaryBallStructuringElement.h"
#include "itkConnectedComponentImageFilter.h"
#include "itkLabelShapeImageFilter.h"

#include "itkSimpleFilterWatcher.h"

#include <itkBinaryThresholdImageFilter.h>
#include <itkExtractImageFilter.h>
#include <itkImageDuplicator.h>

#include <itkImageRegionIterator.h>
#include <itkNeighborhoodAlgorithm.h>

//#define MORPHGRAD

#ifdef MORPHGRAD
#include "itkNeighborhood.h"
#include "itkMorphologicalGradientImageFilter.h"
#else
#include <itkGradientRecursiveGaussianImageFilter.h>
#include <itkGradientToMagnitudeImageFilter.h>
#endif



#include "itkMorphologicalWatershedFromMarkersImageFilter.h"

#include <iostream>
#include <string>
#include <sstream>

#include <list>
#include <vector>
#include <set>
#include <algorithm>
// global defs
const int dim = 3;

typedef unsigned char PType;
typedef float FType;

typedef itk::Image< PType, dim > IType;
typedef itk::Image< FType, dim > FIType;
typedef itk::Image< PType, dim-1 > SliceType;

typedef unsigned short LType;
typedef itk::Image< LType, dim-1 > LabSliceType;

typedef itk::LabelShapeImageFilter<LabSliceType> ShapeStatsType;

typedef std::vector<ShapeStatsType::CenterOfGravityType> COGVecType;
typedef std::vector<unsigned long> VolVecType;

typedef std::list<COGVecType> SliceCOGListType;
typedef std::list<VolVecType> SliceVolListType;
typedef std::list<std::vector<LType> > SliceLabListType;

typedef IType::IndexType Location;

Location::LexicographicCompare CompLoc;

typedef std::set<Location, Location::LexicographicCompare> LSetType;
typedef std::vector<LSetType> NeighbourStructType;
typedef std::vector<Location> LVecType;

int findBiggest(NeighbourStructType N)
{
  int sz = -1;
  int best = -1;
  for (int i = 0; i < N.size(); i++)
    {
    int ts = N[i].size();
    if (ts > sz)
      {
      sz = ts;
      best = i;
      }
    }
  return best;
}


void prune(NeighbourStructType &N, int best)
{
  LSetType rem = N[best];
  for (int i = 0; i < N.size();i++)
    {
    LSetType Replace;
    std::set_difference(N[i].begin(), N[i].end(), rem.begin(), rem.end(), 
			std::insert_iterator<LSetType>(Replace, Replace.begin()),CompLoc);
    N[i] = Replace;
    }
}

LVecType findMarker(const SliceLabListType SliceLabList, 
		    const SliceVolListType SliceVolList,
		    const SliceCOGListType SliceCOGList)
{
  // print everything
  SliceLabListType::const_iterator I1;
  SliceVolListType::const_iterator I2;
  SliceCOGListType::const_iterator I3;



  LVecType LVec;

  int slice = 0;
  int volumeThresh = 5000;
  for (I1 = SliceLabList.begin(), I2 = SliceVolList.begin(), I3 = SliceCOGList.begin();
       I1 != SliceLabList.end(); 
       ++I1, ++I2, ++I3)
    {
    int counter = 0;
    for (int i=0; i<(*I1).size(); i++)
      {
      if (((*I2)[i] > volumeThresh) && ((*I1)[i] != 0))
	{
	++counter;
//	std::cout << "Volume " << (*I2)[i] << std::endl;
	Location L;
	ShapeStatsType::CenterOfGravityType G = (*I3)[i];
	// casting happens here
	L[0]=(long int)G[0];
	L[1]=(long int)G[1];
	L[2]=slice;
	LVec.push_back(L);
	}
      }
    std::cout << "Slice number " << slice << std::endl;
    std::cout << (*I1).size() << " Objects found" << std::endl;
    std::cout << counter << " large enough" << std::endl;
    ++slice;
    }
  // Now cluster based on location - should use a more systematic
  // clustering method, but for now we'll just make something up

  // find all points that are within a certain distance of k others
  const float okdist = 30.0;    // control parameter
  NeighbourStructType Neighbours;
  for (int i=0;i<LVec.size();i++)
    {
    Location A = LVec[i];
    LSetType NVec;
    // Insert the "centre" of the neighborhood
    NVec.insert(A);
    int neighbours = 0;
    for (int j=0; j<LVec.size(); j++) 
      {
      if (i!=j) 
	{
	Location B = LVec[j];
	float xydist = sqrt(pow(float(A[0]) - float(B[0]),2) + pow(float(A[1]) - float(B[1]),2));
//	std::cout << A << B << " " << xydist << std::endl;
	if (xydist <= okdist)
	  {
	  ++neighbours;
	  NVec.insert(B);
	  }
	}
      }
    Neighbours.push_back(NVec);
    std::cout << "slice : " << A[2] << A << " has " << neighbours << " neighbours" << std::endl;
    }

  const int NCount = 8;  // minimum number of neighbors necessart
  LVecType centres;
  for (;;)
    {
    int best = findBiggest(Neighbours);
    int sz = Neighbours[best].size();
    Location ThisCentre = LVec[best];
    std::cout << "Best " << sz << " " << ThisCentre << std::endl;
    if (sz <= NCount) break;
    // Now remove these locations from all other neighbourhoods
    centres.push_back(ThisCentre);
    prune(Neighbours, best);
    }
  return(centres);
}

template <class ImType>
typename ImType::Pointer FillBlock(typename ImType::Pointer InIm, 
				   typename ImType::IndexType centre, 
				   typename ImType::SizeType radius, 
				   typename ImType::PixelType value)
{
  typedef typename itk::ImageRegionIterator<ImType> ItType;
  
  typedef typename ImType::RegionType RegType;
  
  RegType RR;
  for (int y =0; y<ImType::ImageDimension;y++)
    {
    RR.SetSize(y, 2*radius[y]);
    RR.SetIndex(y, centre[y] - radius[y]);
    }
  ItType iter(InIm, RR);
  iter.GoToBegin();
  while (!iter.IsAtEnd())
    {
    iter.Set(value);
    ++iter;
    }
}

// a side filling function -- create the start of a marker image
template <class ImType>
typename ImType::Pointer FillSides(typename ImType::Pointer InIm, typename ImType::PixelType value)
{
  typedef typename itk::ImageDuplicator<ImType> idupType;
  typename idupType::Pointer dup;
  dup = idupType::New();
  dup->SetInputImage(InIm);
  dup->Update();
  typename ImType::Pointer result = dup->GetOutput();

  typename ImType::RegionType reg = result->GetLargestPossibleRegion();
  typename ImType::RegionType SS;
  typename ImType::RegionType::SizeType sz = reg.GetSize();
  typename ImType::RegionType::IndexType st = reg.GetIndex();

  typedef typename itk::NeighborhoodAlgorithm::ImageBoundaryFacesCalculator<ImType> FaceCalculatorType;
  typedef typename FaceCalculatorType::FaceListType FaceListType;
  typedef typename FaceCalculatorType::FaceListType::iterator FaceListTypeIt;

  FaceCalculatorType faceCalculator;

  result->FillBuffer(0);

  FaceListType faceList;
  FaceListTypeIt fit;
  typename ImType::RegionType::SizeType ksz;
  ksz.Fill(1);
  faceList = faceCalculator(result, result->GetLargestPossibleRegion(), ksz);

  // iterate over the face list-ignore the first one
  fit = faceList.begin();
  ++fit;
  typedef typename itk::ImageRegionIterator<ImType> ItType;

  for (;fit != faceList.end();++fit)
    {
    ItType it(result, (*fit));
    it.GoToBegin();
    while(!it.IsAtEnd())
      {
      it.Set(value);
      ++it;
      }
    }
  return(result);
}


int main(int arglen, char * argv[])
{

  typedef itk::ImageFileReader< IType > ReaderType;
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName( argv[1] );

  // fix spacing
  typedef itk::ChangeInformationImageFilter<IType> ChangeInfoType;
  ChangeInfoType::SpacingType spacing;
  spacing.Fill(1);
  spacing[2] = 3;
  ChangeInfoType::Pointer changeInfo = ChangeInfoType::New();
  changeInfo->SetInput(reader->GetOutput());
  changeInfo->SetOutputSpacing(spacing);
  changeInfo->SetChangeSpacing(true);

  IType::Pointer inputimage = changeInfo->GetOutput();

  // morphological prefiltering
  // start with a small opening to remove background speckle
  typedef itk::BinaryBallStructuringElement< PType, dim > SRType;
  typedef itk::GrayscaleErodeImageFilter<IType, IType, SRType> ErodeType;
  typedef itk::GrayscaleDilateImageFilter<IType, IType, SRType> DilateType;

  SRType smallkernel;
  SRType::RadiusType smallrad = smallkernel.GetRadius();
  smallrad.Fill(2);
  smallrad[2]=1;
  smallkernel.SetRadius(smallrad);
  smallkernel.CreateStructuringElement();
  ErodeType::Pointer smallErode = ErodeType::New();
  DilateType::Pointer smallDilate = DilateType::New();
  smallErode->SetKernel(smallkernel);
  smallDilate->SetKernel(smallkernel);
  smallErode->SetInput(inputimage);
  smallDilate->SetInput(smallErode->GetOutput());

  // bigger closing
  SRType bigkernel;
  SRType::RadiusType bigrad = bigkernel.GetRadius();
  bigrad.Fill(9);
  bigrad[2]=3;
  bigkernel.SetRadius(bigrad);
  bigkernel.CreateStructuringElement();
  ErodeType::Pointer bigErode = ErodeType::New();
  DilateType::Pointer bigDilate = DilateType::New();
  bigErode->SetKernel(bigkernel);
  bigDilate->SetKernel(bigkernel);
  // there are actually 2 dilations in cascade - could merge them
  bigDilate->SetInput(smallDilate->GetOutput());
  bigErode->SetInput(bigDilate->GetOutput());

  // now threshold
  typedef itk::BinaryThresholdImageFilter<IType, IType> ThreshType;
  ThreshType::Pointer thresh = ThreshType::New();
  thresh->SetInput(bigErode->GetOutput());
  thresh->SetUpperThreshold(30);
  thresh->SetOutsideValue(255);
  thresh->SetInsideValue(0);
  thresh->Update();
  // a complex cell splitting procedure. The standard distance
  // transform approach doesn't work when the cells are squashed
  // together.
  // We are looking for markers by taking slices through the cell
  //
  // Convert to 2D, one slice at a time
  typedef itk::ExtractImageFilter<IType, SliceType> SlicerType;
  SlicerType::Pointer slicer = SlicerType::New();
  slicer->SetInput(thresh->GetOutput());

  IType::RegionType InRegion = inputimage->GetLargestPossibleRegion();
  IType::SizeType InSize = InRegion.GetSize();

  typedef itk::ImageFileWriter<SliceType> SliceWriterType;
  SliceWriterType::Pointer slicewriter = SliceWriterType::New();
//  typedef itk::ImageFileWriter<LabSliceType> SliceWriterType;
//  SliceWriterType::Pointer slicewriter = SliceWriterType::New();



  // label and shape statistics
  typedef itk::ConnectedComponentImageFilter<SliceType, LabSliceType> LabellerType;
  LabellerType::Pointer labeller = LabellerType::New();

  ShapeStatsType::Pointer shapemeasure = ShapeStatsType::New();

  
  labeller->SetInput(slicer->GetOutput());
  shapemeasure->SetInput(labeller->GetOutput());

  slicewriter->SetInput(slicer->GetOutput());
//  slicewriter->SetInput(labeller->GetOutput());

  SliceCOGListType SliceCOGs;
  SliceVolListType SliceVols;
  SliceLabListType SliceLabs;

  for (int slice = 0; slice < InSize[2]; ++slice)
    {
    COGVecType COGVec;
    VolVecType VolVec;
    std::ostringstream   filename, number;
    IType::RegionType ExtractionRegion = InRegion;
    ExtractionRegion.SetSize(2, 0);
    ExtractionRegion.SetIndex(2,slice);
    slicer->SetExtractionRegion(ExtractionRegion);
    // filter each slice - simply run the pipeline
    shapemeasure->Update();
    const std::vector<LType> Labs = shapemeasure->GetLabels();
    for (int ll = 0;ll < Labs.size(); ll++)
      {
      COGVec.push_back(shapemeasure->GetCenterOfGravity(Labs[ll]));
      VolVec.push_back(shapemeasure->GetVolume(Labs[ll]));
      }
    SliceCOGs.push_back(COGVec);
    SliceVols.push_back(VolVec);
    SliceLabs.push_back(Labs);
    // create the output file name
#if 1
    number.width(2);
    number.fill('0');
    number << slice;
    filename << "slice_" << number.str() << ".tif";
    slicewriter->SetFileName(filename.str().c_str());
    slicewriter->Update();
#endif
    }
  // find the markers
  LVecType centres = findMarker(SliceLabs, SliceVols, SliceCOGs);
  if (centres.size() == 0) 
    {
    std::cout << "Error - failed to find centres" << std::endl;
//    return 1;
    }
    std::cout << "--------------" << std::endl;
  // Now get ready for watershed segmentation
  // create the marker image
  // We'll begin with a background marker that is simply the image
  // border - dodgy, but usually effective
  IType::Pointer marker = FillSides<IType>(inputimage,1);
  IType::SizeType radius;
  radius.Fill(10);
  radius[2]=3;
  for (int YY=0;YY<centres.size();YY++)
    {
    FillBlock<IType>(marker, centres[YY], radius, YY+2);
    
    }

  typedef itk::ImageFileWriter<IType> WriterType;
  WriterType::Pointer writer3d = WriterType::New();
  typedef itk::ImageFileWriter<FIType> WriterType2;
  WriterType2::Pointer writer3d2 = WriterType2::New();

  // compute a gradient of the control image
#ifdef MORPHGRAD
  typedef itk::BinaryBallStructuringElement< PType, dim > KType;
  typedef itk::MorphologicalGradientImageFilter<IType, IType, KType> GradFiltType;
  GradFiltType::Pointer grdMag = GradFiltType::New();
  GradFiltType::KernelType kern;
  kern.SetRadius(1);
  kern.CreateStructuringElement();
  grdMag->SetKernel(kern);
  grdMag->SetInput(bigErode->GetOutput());
#else
  typedef itk::Image< itk::CovariantVector< itk::NumericTraits< PType>::RealType, dim >, dim > GradImType;
  typedef itk::GradientRecursiveGaussianImageFilter<IType,GradImType> GradFiltType;
  typedef itk::GradientToMagnitudeImageFilter<GradImType, FIType> GradMagType;
  GradFiltType::Pointer grd = GradFiltType::New();
  GradMagType::Pointer grdMag = GradMagType::New();
  grd->SetInput(bigErode->GetOutput());
  grd->SetSigma(2);
  grdMag->SetInput(grd->GetOutput());
#endif
  
  typedef itk::MorphologicalWatershedFromMarkersImageFilter<FIType, IType> WShedType;
  WShedType::Pointer wshed = WShedType::New();
  wshed->SetInput(grdMag->GetOutput());
  wshed->SetMarkerImage(marker);
  wshed->SetFullyConnected(false);
  wshed->SetMarkWatershedLine(false);
  wshed->Update();

  writer3d->SetInput(marker);
  writer3d->SetFileName("marker.tif");
  writer3d->Update();

  writer3d->SetInput(bigErode->GetOutput());
  writer3d->SetFileName("preprocessed.tif");
  writer3d->Update();

  writer3d2->SetInput(grdMag->GetOutput());
  writer3d2->SetFileName("gradient.img");
  writer3d2->Update();

  writer3d->SetInput(wshed->GetOutput());
  writer3d->SetFileName("segresult.tif");
  writer3d->Update();
  return 0;
}

