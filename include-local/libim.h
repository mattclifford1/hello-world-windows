#include <iostream>
#include <vector>

class imageCopy
{
  private:
    int n_rows;
    int n_cols;
    int n_chans;
    int vec_ind;
    // data stored in RGB chunks going along rows within cols
    int getInd(int row, int col, int chan){
      return (row*n_cols*n_chans + col*n_chans + chan);}
  public:
    imageCopy(int rows, int cols, int chans);
    std::vector<float> imageVec;

    void initCopy(float value);
    void setPix(int row, int col, int chan, float value);

    float getPix(int row, int col, int chan);
    void printIm();

    int getNumRows(){return n_rows;}
    int getNumCols(){return n_cols;}
    int getNumChans(){return n_chans;}
};
