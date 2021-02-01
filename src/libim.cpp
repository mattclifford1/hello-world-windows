#include "libim.h"

// constructor
imageCopy::imageCopy(int rows, int cols, int chans){
  n_rows = rows;
  n_cols = cols;
  n_chans = chans;
  vec_ind = 0;
  imageVec.reserve(n_rows * n_cols * n_chans); // preallocate memory for image vector
}

// initial image copying - can just sequentially increment values
void imageCopy::initCopy(float value){
  imageVec[vec_ind] = value;
  vec_ind++;
}

// set a specific pixel
void imageCopy::setPix(int row, int col, int chan, float value){
  if (row>=n_rows || col>=n_cols){
    std::cout<<"Pixel out of bounds\n";
  }
  else{
    imageVec[getInd(row, col, chan)] = value;
  }
}

float imageCopy::getPix(int row, int col, int chan){
  if (row>=n_rows || col>=n_cols){
    std::cout<<"Pixel out of bounds\n";
    return -1;
  }
  return imageVec[getInd(row, col, chan)];
}

void imageCopy::printIm(){
  for (int chan=0; chan<getNumChans(); chan++){
    for (int row=0; row<getNumRows(); row++){
      for (int col=0; col<getNumCols(); col++){
        std::cout<<getPix(row, col, chan)<<" ";
      }
      std::cout<<"\n";
    }
    std::cout<<"\n";
  }
  std::cout<<"\n";
}
