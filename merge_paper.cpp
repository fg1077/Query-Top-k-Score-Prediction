#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <algorithm>
#include <vector>
#include <tuple>
#include <utility>
#include <string>
#include <math.h>

using namespace std;

const int BLOCKSIZE = 128;



void writelexiconline(ifstream & lexif, ofstream & lexof, ulong bytes)
{
  //copies line from intermediate lexicon and adds inverted index byte offset to
  //start of list for a word
  string lexiconline;
  if(std::getline(lexif, lexiconline))
  {
    istringstream iss(lexiconline);
    unsigned int wordid;
    string word;
    iss >> wordid >> word;
    lexof << wordid << " " << word << " " << bytes << "\n";
  }
}

void encodenum(unsigned int num, unsigned char * bytearray, unsigned int &nbytes)
{
  if (num == 0)
  {
    bytearray[0] = static_cast<unsigned char>(num);
    nbytes = 1;
    return;
  }

  unsigned int numcopy = num;
  unsigned int nbits = 0;

  while(numcopy)
  {
    nbits++;
    numcopy = (numcopy >> 1);
  }


  //cout << "nbits: " << nbits << "\n";

  nbytes = 1;
  while(floor(num/pow(128, nbytes)) > 0)
  {
    nbytes++;
  }
  //cout << "nbytes: " << nbytes << "\n";



  for (int i = 0; i < nbytes; i++)
  {
    if (i < (nbytes -1))
      bytearray[i] = static_cast<unsigned char>(((num >> 7*i) | 0b10000000));
    else
      bytearray[i] = static_cast<unsigned char>(((num >> 7*i) & 0b01111111));
  }

}

void updatebuffer(unsigned int num, tuple<unsigned int, unsigned int> & element, unsigned int & bytecount)
{
  //saves directly to an element in the output array the varbyte encoded version
  //of a number
  //also directly increments count of total number of bytes in buffer

  unsigned char bytearray[5];
  unsigned int nbytes;
  encodenum(num, bytearray, nbytes);

  unsigned int encodednum =0;
  for (int i = 0; i < nbytes; i++)
    encodednum = encodednum + (static_cast<unsigned int>(bytearray[i]) << 8*i);

  tuple<unsigned int, unsigned int> encodedtuple;

  get<0>(encodedtuple) = encodednum;
  get<1>(encodedtuple) = nbytes;

  element = encodedtuple;
  bytecount = bytecount + get<1>(encodedtuple);
}

void inttobytearray(unsigned int num, unsigned char bytearray [], unsigned int & nbytes)
{
  unsigned int j = 0;

  if (num == 0)
  {
    nbytes = 1;
    bytearray[0] = static_cast<unsigned char>(0);
    return;
  }

  while((num >> 8*j) > 0 && j < 4)
  {
    bytearray[j] = static_cast<unsigned char>((num >> 8*j));
    j++;

    //if(j > 4)
      //cout << "num: " << num << " Bytearray overflow in inttobytearray\n";
  }

  nbytes = j;
  return;
}

ulong writeencodedblocks(ofstream &fs, int n, unsigned int docid [], unsigned int freq [])
  //encodes and writes differences of blocks of docid's and frequencies

  {
    tuple<unsigned int, unsigned int> dociddif[BLOCKSIZE];
    tuple<unsigned int, unsigned int>freqdif[BLOCKSIZE];

    for (int i = 0; i < BLOCKSIZE; i++)
    {
      get<0>(dociddif[i]) = 0;
      get<1>(dociddif[i]) = 0;

      get<0>(freqdif[i]) = 0;
      get<1>(freqdif[i]) = 0;
    }

    unsigned int nbytesdocid = 0;
    unsigned int nbytesfreq = 0;

    updatebuffer(docid[0], dociddif[0], nbytesdocid);
    updatebuffer(freq[0], freqdif[0], nbytesfreq);
    for (int i = 1; i < n; i++)
    {
      updatebuffer(docid[i] - docid[i-1], dociddif[i], nbytesdocid);
      updatebuffer(freq[i], freqdif[i], nbytesfreq); //frequencies are not compressed as differences
    }

    //cout << nbytesdocid << " " << nbytesfreq << "\n";

    //write metadata and blocks of differences to inverted index file
    fs.write((char*)&(docid[n-1]), sizeof(unsigned int)); //last docid
    fs.write((char*)&(nbytesdocid), sizeof(unsigned int)); //number of bytes in docid difference block
    fs.write((char*)&(nbytesfreq), sizeof(unsigned int)); //number of bytes in freq difference block

    //write byte-by-byte differences
    unsigned char bytearray[5];
    unsigned int nbytes;

    //docid's
    for (int i=0; i < n; i++)
    {
      //encodenum(get<0>(dociddif[i]), bytearray, nbytes);
      inttobytearray(get<0>(dociddif[i]), bytearray, nbytes);
      //cout << "docid " << get<0>(dociddif[i]) << " nbytes " << nbytes << " i " << i << "\n";

      fs.write((const char*)bytearray, nbytes);
    }

    //freq's
    for (int i = 0; i < n; i++)
    {
      //encodenum(get<0>(freqdif[i]), bytearray, nbytes);
      //cout << get<0>(freqdif[i]) << "\n";
      inttobytearray(get<0>(freqdif[i]), bytearray, nbytes);

      //cout << "freq " << get<0>(freqdif[i]) << " nbytes " << nbytes << " i " << i << "\n";

      fs.write((const char*)bytearray, nbytes);

    }

    ulong returnvalue = 12 + nbytesdocid + nbytesfreq;

    return returnvalue;

  }

int main()
{

  //////////////////try to open and read metadata file////////
  std::ifstream metadatais("./postings/postingmetadata.txt");
  if (!metadatais.is_open())
  {
    std::cout<<"Unable to open metadata file";
    return 0;
  }

  string line;

  if(!std::getline(metadatais,line))
  {
    std::cout<<"Unable to read metadata file";
    return 0;
  }

  //read metadata file
  string placeholder;
  istringstream ss;
  unsigned int npostingfiles;
  unsigned int npostings;
  unsigned int nwords;
  unsigned int ndocs;
  float avgdoclength;

  ss.str(line);
  ss >> placeholder >> npostingfiles;
  getline(metadatais, line);
  ss >> placeholder >> npostings;
  getline(metadatais, line);
  ss >> placeholder >> nwords;
  getline(metadatais, line);
  ss >> placeholder >> ndocs;
  getline(metadatais, line);
  ss >> placeholder >> avgdoclength;
  ss.clear();

  std::cout << "Number of posting files: " << npostingfiles << "\n";
  std::cout << "Number of postings: " << npostings << "\n";
  std::cout << "Number of words: " << nwords << "\n";
  std::cout << "Number of documents: " << ndocs << "\n";
  std::cout << "Average document length in words " << avgdoclength << "\n";
  //////////create arrays and heap for reading in and merging postings ////

  std::queue<tuple<unsigned int, unsigned int, unsigned int, unsigned int>>* postingqueuepointers[npostingfiles];

  int buffertotalbytes = 10000000; //combined buffer size for postings queues
  int bufferlength = buffertotalbytes/npostingfiles/4/4;

  if (bufferlength < 5)
  {
    std::cout << "Number of postings in each read buffer is less than 5 (" << bufferlength <<").\n"
    <<"Increase size of buffer or decrease the number of posting files\n";
    return 0;
  }
  else
    std::cout <<"Buffer length: " << bufferlength << "\n";


  ////////////////////////////////////////////////////////
  //////////load posting queues//////////////////////

  //open files for reading
  ifstream is[npostingfiles]; //array of file handles for easy indexing
  string fn;
  for (int i = 0; i < npostingfiles; i++)
  {
    fn = "./postings/" + to_string(i+1) + ".dat";
    is[i].open(fn, ios::binary);
    if(is[i])
      std::cout <<fn << " opened successfully\n";
    else
    {
      std::cout << fn << " failed to open, exiting";
      return 0;
    }
  }


  //unsigned int readint;
  //tuples of read data and queue index (wordid, docid, freq, queue index)
  tuple<unsigned int, unsigned int, unsigned int, unsigned int> readtuple;
  unsigned int temp;
  for (unsigned int i = 0; i < npostingfiles; i++)
  {
    postingqueuepointers[i] = new queue<tuple<unsigned int, unsigned int, unsigned int, unsigned int>>;

    std::cout <<"Reading from " << i+1 << ".dat\n";
    for (int j = 0; j < bufferlength; j++)
    {

      is[i].read(reinterpret_cast<char*>(&temp), sizeof(unsigned int)); //read four bytes and cast to unsigned integer
      if (is[i].eof()) //check if previous read attempt hit eof flag
        break;
      //std::cout << "Initial reading for queue " << i << "/" << npostingfiles-1 <<
      //", entry " << j << "/" << bufferlength-1 << ": ";
      //std::cout << temp << " ";
      std::get<0>(readtuple) = temp; //save word ID in tuple
      is[i].read(reinterpret_cast<char*>(&temp), sizeof(unsigned int));
      //std::cout << temp << " ";
      std::get<1>(readtuple) = temp; //save document ID in tuple
      is[i].read(reinterpret_cast<char*>(&temp), sizeof(unsigned int));
      //std::cout << temp << "\n";
      //std::cout << is[i].eof() << "\n";
      std::get<2>(readtuple) = temp;  //save frequency in tuple
      std::get<3>(readtuple) = i;   //save queue index in tuple
      postingqueuepointers[i]->push(readtuple); //push tuple into queue


    }

  }
  std::cout<<"Read in " << bufferlength << " postings for " << npostingfiles << "queues\n";
  //////make heap////////


  //////////initialize min-heap
  ///elements are tuples (word ID, document ID, frequency, queue ID)
  std::priority_queue<tuple<unsigned int, unsigned int, unsigned int, unsigned int>,
  std::vector<tuple<unsigned int, unsigned int, unsigned int, unsigned int>>,
  std::greater<tuple<unsigned int, unsigned int, unsigned int, unsigned int>>> pq;

  for (unsigned int i = 0; i < npostingfiles; i++)  //push first element of each queue into min-heap
  {
    pq.push(postingqueuepointers[i]->front());
    postingqueuepointers[i]->pop();

  }

  std::cout << "Min-Heap constructed\n";

  //open output file stream
  std::ofstream wf("invertedindex.dat", std::ios::binary);
  if (!wf)
  {
    cout << "Unable to open output file";
    return 0;
  }

  //open intermediate lexicon file
  std::ifstream lexiconif("lexicon.txt");
  if (!lexiconif)
  {
    cout << "Unable to open intermediate lexicon file lexicon.txt";
    return 0;
  }

  //open output lexicon file
  std::ofstream lexiconof("lexicon_final.txt");
  if(!lexiconof)
  {
    cout << "Unable to open final lexicon output file lexicon_final.txt";
    return 0;
  }

  //put postings through min heap and write out merged posting and final lexicon
  //in final lexicon, each word has number of previous entries
  //to allow random access

  std::tuple<unsigned int, unsigned int, unsigned int, unsigned int> popped;
  unsigned int queueindex;
  unsigned int previouswordid = 1;
  unsigned int wordid;
  //std::string lexiconline;

  ulong byteoffset = 0;
  //int numdocsforaword = 0; //keeps a count of number
  int npoppedpostings = 0;

  //output buffer variables
  int nelements = 0;

  unsigned int docidbuffer [BLOCKSIZE];
  unsigned int freqbuffer [BLOCKSIZE];

  std::cout<<"Writing postings...\n";

  /////write first line of final lexicon////
  writelexiconline(lexiconif, lexiconof, 0);

  while (!pq.empty()) //pop min-heap
  {
    ///elements are tuples (word ID, document ID, frequency, queue ID)
    popped = pq.top();
    pq.pop();
    queueindex = std::get<3>(popped);
    wordid = std::get<0>(popped);
    //val = popped.first;

    npoppedpostings = npoppedpostings + 1;

    if (npoppedpostings%1000000 == 0)
      std::cout << "Popped " << npoppedpostings << " postings\n";

    //std::cout << "Popped " << npoppedpostings << "/" <<npostings << ": "  <<
    //wordid << ", " << std::get<1>(popped) << ", " <<
    //std::get<2>(popped) << ", " << queueindex << "\n";

    //if buffer full, write to file

    if (wordid != previouswordid)
    {

      //cout << "wordid: " << wordid << " nelem: " << nelements << " last docid: " << docidbuffer[nelements] << "\n";
      //cout << docidbuffer[0] << " " << docidbuffer[1] << " " << docidbuffer[2] << "\n";

      byteoffset = byteoffset + writeencodedblocks(wf, nelements, docidbuffer, freqbuffer);
      nelements = 0;
      previouswordid = wordid;

      writelexiconline(lexiconif, lexiconof, byteoffset);
      //std::cout << "New wordid found, writing offset for new wordid " << wordid << "\n";
    }

    if (nelements == BLOCKSIZE)
    {
      byteoffset = byteoffset + writeencodedblocks(wf, nelements, docidbuffer, freqbuffer);
      nelements = 0;
    }

    docidbuffer[nelements] = get<1>(popped);
    freqbuffer[nelements] = get<2>(popped);
    nelements = nelements + 1;

    //write document ID and frequency to inverted index
    //wf.write((char*)&(get<1>(popped)), sizeof(unsigned int));
    //wf.write((char*)&(get<2>(popped)), sizeof(unsigned int));

    //numdocsforaword = numdocsforaword + 1; //increment offset


    //reload a queue
    if(postingqueuepointers[queueindex]->empty())
    {

      //std::cout <<"Queue " << queueindex << " empty, reloading...\n";
      for (int i = 0; i < bufferlength; i++)
      { //load bufferlength tuples into queue
        //same process as initial loading

        is[queueindex].read(reinterpret_cast<char*>(&temp), sizeof(unsigned int));
        if (is[queueindex].eof())
        {
          std::cout << queueindex + 1 << ".dat is empty, closing file and continuing\n";
          is[queueindex].close();
          break;
        }
        //std::cout << "Read for reloading queue " << queueindex << ", ";
        //std::cout << temp << " ";
        std::get<0>(readtuple) = temp;
        is[queueindex].read(reinterpret_cast<char*>(&temp), sizeof(unsigned int));
        //std::cout << temp << " ";
        std::get<1>(readtuple) = temp;
        is[queueindex].read(reinterpret_cast<char*>(&temp), sizeof(unsigned int));
        //std::cout << temp << "\n";
        //std::cout << is[queueindex].eof() << "\n";
        std::get<2>(readtuple) = temp;
        std::get<3>(readtuple) = queueindex;
        postingqueuepointers[queueindex]->push(readtuple);

      }
    } //end of reloading queue

    if (!postingqueuepointers[queueindex]->empty())
    { //if queue not empty, pop
      popped = postingqueuepointers[queueindex]->front();
      postingqueuepointers[queueindex]->pop();
      pq.push(popped);
    }

    //end of priority queue loop
  }

  //write any remaining docid's and frequencies to file
  if (nelements > 0)
  {
    byteoffset = byteoffset + writeencodedblocks(wf, nelements, docidbuffer, freqbuffer); //should be last word
    nelements = 0;
  }

  std::cout << "Posting files merged\n";

  //if(std::getline(lexiconif, lexiconline))
    //lexiconof << lexiconline.substr(0, lexiconline.size()-1) << " " << numdocsforaword << "\n";

  //std::cout << "Final Lexicon file written\n";
  //close files
  wf.close();
  lexiconif.close();
  //lexiconof.close();

  return 0;


}
