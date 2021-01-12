#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>
#include <queue>
#include <math.h>
#include <iomanip>
#include <stdlib.h>

const int BLOCKSIZE = 128;
const int KRESULTS = 10;
const bool DEBUG = true;

using namespace std;

struct md
{
  unsigned int n_posting_files;
  unsigned int n_postings;
  unsigned int n_words;
  unsigned int n_docs;
  float avg_doc_length;
} metadata;

struct lexiconelement
{
  ulong byteoffset;
  ulong nbytes;
  unsigned int numdocsthatcontainword;
};

struct pagetableelement
{
  string url;
  unsigned int numwordsindoc;
};

typedef unordered_map<string, lexiconelement> lexicontype;
//typedef unordered_map<string, ulong> invertedindextype;
typedef unordered_map<unsigned int, pagetableelement> pagetabletype;

lexicontype lexum; //lexicon hashmap
pagetabletype ptum; //pagetable hashmap
ifstream iiis; //inverted index input stream
unsigned char ** queryii; //pointer for dynamic array of inverted index lists for each query word
unsigned int * queryiilen; //pointer for dynamic array of query word inverted index lengths
vector<string> querywords; //vector of query words
priority_queue<pair<double, unsigned int>> pq;
double * docscores;

void printlexicon()
{
  cout << "Lexicon length: " << lexum.size() << "\n";
  for (auto a : lexum)
  {
    cout << "key: " << a.first << " byteoffset: " <<  a.second.byteoffset << " nbytes: "
    << a.second.nbytes << " numdocsthatcontainword: " << a.second.numdocsthatcontainword << "\n";
  }
}

void printpagetable()
{
  cout << "Page table length: " << ptum.size() << "\n";
  for (auto a : ptum)
  {
    cout << "key: " << a.first << " url: " <<  a.second.url << " numwordsindoc: "
    << a.second.numwordsindoc <<  "\n";
  }
}

void printinvertedindexes()
{
  //prints chars as ints
  for (int i = 0; i < querywords.size(); i++)
  {
    cout << querywords[i] << " ";
    for (int j = 0; j < queryiilen[i]; j++)
    {
      cout << static_cast<unsigned int>((queryii[i])[j]) << " ";
    }
    cout << "\n";
  }
}

bool open_files(ifstream & lexis, ifstream & ptis, ifstream & mdis)
{
  //returns false if a file fails to open

  lexis.open("lexicon_final.txt");
  if(!lexis.is_open())
  {
    cout << "lexicon_final.txt unable to be opened\n";
    return false;
  }

  ptis.open("pagetable.txt");
  if(!ptis.is_open())
  {
    cout << "pagetable.txt unable to be opened\n";
    return false;
  }

  iiis.open("invertedindex.dat", ios::binary);
  if(!iiis.is_open())
  {
    cout << "invertedindex.dat unable to be opened\n";
    return false;
  }

  mdis.open("./postings/postingmetadata.txt");
  if(!mdis.is_open())
  {
    cout << "/postings/postingmetadata.txt unable to be opened\n";
    return false;
  }

  return true;
}

void close_files(ifstream & lexis, ifstream & ptis, ifstream & mdis)
{
  lexis.close();
  ptis.close();
  iiis.close();
  mdis.close();
}

void lexicon_to_hash_table(ifstream & lexis)
{
  //<key, val> = <word, <byteoffset in inverted index, number of bytes in list>>

  unsigned int temp;
  string line;
  string word;
  ulong byteoffset;
  istringstream ss;
  string lastword;
  int count = 0;
  //pair <ulong, ulong> temp;
  ulong second = 0;

  while(getline(lexis, line))
  {
    ss.str(line);
    ss >> temp >> word >> byteoffset;
    lexum.insert({word, {byteoffset, 0, 0}});
    ss.clear();

    if(count > 0)
    {
      ulong second = byteoffset - lexum[lastword].byteoffset;  //calculate length of list
      lexum[lastword] = {lexum[lastword].byteoffset, second, lexum[lastword].numdocsthatcontainword};
    }
    lastword = word;
    count++;
  }

}

void pagetable_to_hash_table(ifstream & ptis)
{
  //<key, val> = <docid, url>

  string line;
  unsigned int docid;
  unsigned int numwordsindoc;
  string url;
  istringstream ss;

  while(getline(ptis, line))
  {
    ss.str(line);
    ss >> docid >> url >> numwordsindoc;
    ptum.insert({docid, {url, numwordsindoc}});
    ss.clear();
  }
}

void metadata_to_struct(ifstream & mdis)
{
  string line;
  string temp;
  istringstream ss;

  getline(mdis, line);
  ss.str(line);
  ss >> temp >> metadata.n_posting_files;
  ss.clear();

  getline(mdis, line);
  ss.str(line);
  ss >> temp >> metadata.n_postings;
  ss.clear();

  getline(mdis, line);
  ss.str(line);
  ss >> temp >> metadata.n_words;
  ss.clear();

  getline(mdis, line);
  ss.str(line);
  ss >> temp >> metadata.n_docs;
  ss.clear();

  getline(mdis, line);
  ss.str(line);
  ss >> temp >> metadata.avg_doc_length;
  ss.clear();

}

void print_metadata()
{
  cout << "n_posting_files: " << metadata.n_posting_files << "\n";
  cout << "n_postings: " << metadata.n_postings << "\n";
  cout << "n_words: " << metadata.n_words << "\n";
  cout << "n_docs: " << metadata.n_docs << "\n";
  cout << "avg_doc_length: " << metadata.avg_doc_length << "\n";
}

void getqueryterms(string querystring)
{
  istringstream ss(querystring);

  string word;
  bool repeated;

  while(ss >> word)
  {
    repeated = false;
    for(int i = 0; i < querywords.size(); i++)
    {
      if (word == querywords[i])
      {
        repeated = true;
        break;
      }
    }
    if (!repeated)
      querywords.push_back(word);
  }

  if (DEBUG)
  {
    cout << "Query words: \n";
    for (int i = 0; i < querywords.size(); i++)
      cout << querywords[i] << " ";
    cout << "\n";
  }
}

void getinvertedindexes()
{
  unsigned int nwords = querywords.size();


  queryii = new unsigned char * [nwords];
  queryiilen = new unsigned int [nwords];

  unsigned int nbytes;
  for (int i = 0; i < nwords; i++)
  {
    queryii[i] = new unsigned char [lexum[querywords[i]].nbytes];
    iiis.seekg(lexum[querywords[i]].byteoffset, iiis.beg);
    iiis.read((char *)queryii[i], lexum[querywords[i]].nbytes);
    queryiilen[i] = lexum[querywords[i]].nbytes;
  }

  if (DEBUG)
    cout << "End of getinvertedindexes()\n";
}

void deletedynamicarrays()
{
  for (int i = 0; i < querywords.size(); i++)
  {
    delete [] queryii[i];
  }
  if (DEBUG)
    cout << "Deleted individual inverted index lists\n";

  delete [] queryii;
  if (DEBUG)
    cout << "Deleted umbrella inverted index array\n";
  delete [] queryiilen;
  if (DEBUG)
    cout << "Deleted inverted index length array\n";
}


unsigned int bytearraytoint(unsigned char * ptr, unsigned int numbytes)
{

  unsigned int num = 0;
  for (int i = 0; i < numbytes; i++)
    num += (static_cast<unsigned int>(ptr[i]) << i*8);

  return num;
}

unsigned int countnumdocswithword(string word, int queryiiindex)
{
  //every word must have at least one doc that contains it
  //updates lexicon numdocswithword if it is zero


  if (lexum[word].numdocsthatcontainword == 0)
  {
    if (lexum[word].nbytes == 0)
      return 1000000; //make bm25 score small


    unsigned int doccount = 0;
    ulong index = 0;
    unsigned int docidbytes = 0;
    unsigned int freqbytes = 0;
    unsigned char byte;
    while (index < lexum[word].nbytes)
    {

      //skip last docid
      index = index + 4;
      //get number of bytes in docid block

      docidbytes = bytearraytoint(queryii[queryiiindex] + index, 4);
      index = index + 4;

      //get number of bytes in frequency block

      freqbytes = bytearraytoint(queryii[queryiiindex] + index, 4);
      index = index + 4;

      //count number of bytes with 0 prefix
      for (int i = 0; i < docidbytes; i++)
      {
        byte = *(queryii[queryiiindex] + index);
        index++;

        if ((static_cast<unsigned int>(byte) >> 7) == 0)
          doccount++;
      }

      //skip frequencies
      index = index + freqbytes;
    }

    lexum[word].numdocsthatcontainword = doccount;

    if (DEBUG)
    {
      cout << "countnumdocswithword(): " << word << " " << queryiiindex << " " << doccount << "\n";
    }

    return doccount;

  }

  return lexum[word].numdocsthatcontainword;
}

unsigned int decompressvarbyte(unsigned char * iiptr, unsigned int & nbytes)
{

  //varbyte decompression
  unsigned int num = 0;
  unsigned char * ptr = iiptr;
  int i = 0;


  num += ((static_cast<unsigned int>(ptr[i]) & 0b01111111) << 7*i);
  while(((static_cast<unsigned int>(ptr[i])>> 7) == 1) && i < 5)
  {
    i++;
    num += ((static_cast<unsigned int>(ptr[i]) & 0b01111111) << 7*i);
  }
  i++;

  nbytes = i;

  return num;
}


double getbm25part(string word, unsigned int docid, int queryiiindex, unsigned int fdt)
{

  double nwords = static_cast<double>(ptum[docid].numwordsindoc);
  double bm25part = 0.0;
  double K = 1.2*(0.25 + 0.75*nwords/metadata.avg_doc_length);
  unsigned int doccount = countnumdocswithword(word, queryiiindex);
  bm25part = log((metadata.n_words - doccount + 0.5)/(doccount + 0.5))*2.2*fdt/(K + fdt);

  return bm25part;

}

void printdocumentscores()
{
  cout << "Printing document scores:\n";
  for (int i = 0; i < metadata.n_docs; i++)
    cout << docscores[i] << "\n";
}

void getdocumentscoresdisjunctive()
{
  docscores = new double[metadata.n_docs];
  for (int i = 0; i < metadata.n_docs; i++)
    docscores[i] = 0.0;

  unsigned int currentdocid;
  unsigned int currentfreq;
  unsigned int nbytesdocid;
  unsigned int nbytesfreq;
  unsigned int index = 0;
  unsigned int temp;
  unsigned int readbytesdocid;
  unsigned int readbytesfreq;

  for (int i = 0; i < querywords.size(); i++)
  {

    index = 0;
    while (index < lexum[querywords[i]].nbytes)
    {
      //skip last doc id
      index +=4;
      //read number of doc id bytes

      nbytesdocid = bytearraytoint(queryii[i] + index, 4);
      index += 4;

      //read number of freq bytes
      nbytesfreq = bytearraytoint(queryii[i] + index, 4);
      index += 4;

      readbytesdocid = 0;
      readbytesfreq = 0;
      currentdocid = 0;
      currentfreq = 0;
      while(readbytesdocid < nbytesdocid && readbytesfreq < nbytesfreq)
      {
        currentdocid += decompressvarbyte((queryii[i] + index + readbytesdocid), temp);

        readbytesdocid += temp;
        currentfreq = decompressvarbyte((queryii[i] + index + nbytesdocid + readbytesfreq), temp); //frequencies not compressed as differences

        readbytesfreq += temp;

        docscores[currentdocid-1] += getbm25part(querywords[i], currentdocid, i, currentfreq);

      }
      index += (nbytesdocid + nbytesfreq);

    }
  }

  if (DEBUG)
    cout << "End of getdocumentscores()\n";
}

void printtopkdocuments(int k)
{
  for (unsigned int i = 0; i < metadata.n_docs; i++)
  {
    pq.push({docscores[i], (i+1)});
  }

  pair<double, unsigned int> temp;

  double score;
  unsigned int docid;
  for (unsigned int i = 0; i < k; i++)
  {
    if(pq.empty())
      break;

    temp = pq.top();
    score = get<0>(temp);
    docid = get<1>(temp);

    cout << i+1 << " " << ptum[docid].url << " " << setprecision(8) << fixed << score << "\n";
    pq.pop();
  }

  if (DEBUG)
    cout << "End of printtopkdocuments()\n";
}

void cleardatastructuresdisjunctive()
{
  delete [] docscores;
  if (DEBUG)
    cout << "Deleted docscores\n";

  while(!pq.empty())
    pq.pop();
  if (DEBUG)
    cout << "Cleared priority queue\n";

  deletedynamicarrays();

  querywords.clear();
}

bool anyfinishedlists(unsigned int * lastpos)
{
  for (int i = 0; i < querywords.size(); i++)
  {
    if (lastpos[i] >= queryiilen[i])
      return true;
  }
  return false;
}


void termatatimedisjunctive(string querystring, int k)
{
  getqueryterms(querystring);

  getinvertedindexes();

  //printinvertedindexes();

  getdocumentscoresdisjunctive();

  //printdocumentscores();

  printtopkdocuments(k);

  cleardatastructuresdisjunctive();

}

int main()
{

//////////////////////////////////////////////////////////////////////
////////open lexicon, pagetable, inverted index, and metadata files///
///////assumes they are in same directory as queryprocess////////////

  cout << "Starting query processor.\n";

  ifstream lexiconis;
  ifstream pagetableis;
  ifstream metadatais;

  if(!open_files(lexiconis, pagetableis, metadatais))
    return 0;

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
//////////load lexicon, pagetable, and metadata into hash tables////


  lexicon_to_hash_table(lexiconis);  //<key, val> = <docid, url>
  lexiconis.close();
  cout << "Loaded lexicon.\n";

  pagetable_to_hash_table(pagetableis);
  pagetableis.close();
  cout << "Loaded pagetable.\n";

  metadata_to_struct(metadatais);
  metadatais.close();
  cout << "Loaded metadata.\n";
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
/////////some testing//////////
//printlexicon();
//printpagetable();

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
/////////main loop//////////

string inputline;
while(true)
{
cout << "ENTER QUERY\n\n";
getline(cin, inputline);
cout << "\nPROCESSING...\n\n";
termatatimedisjunctive(inputline, 10);

}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////


  return 0;
}
