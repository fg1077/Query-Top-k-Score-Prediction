import sys
import time
import re
import os
import shutil
import heapq
import nltk
nltk.download('stopwords')
nltk.download('words')
nltk.download('wordnet')
from nltk.corpus import stopwords
from nltk.stem import PorterStemmer

###############################################################
##########global variables##################################

stop_words = set(stopwords.words('english'))
ps = PorterStemmer()

#word.words() and wordnet.words() are already stemmed
nltk_words = set(nltk.corpus.words.words())
nltk_wordnet = set(nltk.corpus.wordnet.words())
del ps
#################################################################
##############function definitions############################

def get_words(line):
    #returns list of stemmed words
    ps = PorterStemmer()
    temp = re.sub(r"[\[\]`~!@#$%^&*()_+-=|{}:;<>?,./1234567890'\"]", " ", line).lower().strip().split()

    #words = [ps.stem(x) for x in temp if (x not in stopwords.words('english') and (len(x) > 2) and (re.search(r"\W", x) is None))]

    #words = [ps.stem(x) for x in temp if ((len(x) > 2) and (x not in stop_words) and (re.search(r"\W", x) is None))]

    words = [ps.stem(x) for x in temp if ((x not in stop_words) and (x in nltk_words or x in nltk_wordnet))]

    return words

def update_data_structures(text, rawtext, urls, lex, pt, pb, md, fh):

    #text is text from document without tags
    #urls is urllist, first element is document url, if urls is empty, new document will not be saved
    #lex is lexicon
    #pt is page table
    #pb is postings buffer
    #md is metadata
    #fh is fila handle dictionary

    if (not urls):
        print("No url associated with document. Document not created.")
        return

    wordlist = get_words(text)

    if (len(wordlist) == 0):
        #print("No words in document. Document not created")

        md["numnotextdocs"] = md["numnotextdocs"] + 1
        fh["notextdocsfh"].write("Doc. " + str(md["numnotextdocs"]) + "\n")
        #for url in urls:
        #    fh["notextdocsfh"].write(url + "\n")
        #fh["notextdocsfh"].write(text + "\n")
        fh["notextdocsfh"].write(rawtext + "\n")
        return

    fd = {} #frequency dictionary

    #save new words to lexicon and count words' in-document frequencies
    for word in wordlist:
        if word not in lex.keys():
            metadata["numwords"] = metadata["numwords"] + 1
            lex[word] = metadata["numwords"]
            fh["lexiconfh"].write(str(metadata["numwords"]) + " " + word + "\n")

        if word not in fd.keys():
            fd[word] = 1
        else:
            fd[word] = fd[word] + 1

    md["sumdocwords"] =md["sumdocwords"] + len(wordlist)

    #add new entry in page table file
    metadata["numdocs"] = metadata["numdocs"] + 1
    fh["pagetablefh"].write(str(metadata["numdocs"]) + " " + urls[0] + " " + str(len(wordlist)) + "\n")

    #load postings into postings buffer
    for word in fd.keys():
        heapq.heappush(pb, ((lex[word], metadata["numdocs"]), fd[word]))

def write_postings_buffer(pb, md, binary = True):

    #increment number of posting lists and open new posting file
    md["numpostinglists"] = md["numpostinglists"] + 1

    print("Writing " + str(md["numpostinglists"]) + ".dat")

    cwd = os.getcwd()

    if (binary):
        postingfh = open(cwd + "//postings//" +  str(md["numpostinglists"])+ ".dat", "wb+")
    else:
        postingfh = open(cwd + "//postings//" + str(md["numpostinglists"]) + ".txt", "w+")

    while pb:

        tuple = heapq.heappop(pb)
        wordid, docid = tuple[0]
        freq = tuple[1]

        if (binary):
            postingfh.write((wordid).to_bytes(4,byteorder = 'little'))
            postingfh.write((docid).to_bytes(4,byteorder = 'little'))
            postingfh.write((freq).to_bytes(4,byteorder = 'little'))
        else:
            postingfh.write(str(wordid) + " " + str(docid) + " " + str(freq) + "\n")

        md["postingcount"] = md["postingcount"] + 1

    postingfh.close()

###############################################################
############parameters########################################

pwd = os.getcwd()
corpusfp = 'C:\\Users\\frank\\Desktop\\nyu\FA20\\web search engines\\TREC-2019\\fulldocs-new.trec' #assumes uncompressed file in same directory as scrip
doc_limit = 4000000
posting_buffer_size_limit = 30000000 #limit in bytes
posting_buffer_limit = int(160000*3300000/50) #limit in number of postings
binarypostingfiles = True
doc_interval = 80000
###############################################################
##########variables to initialize#############################

cwd = os.getcwd()

tic = time.time()
lexicon = {} #{word: wordid}
pagetable = {}
postingsbuffer = [] #[((wordid, docid), frequency)]

metadata = {}

metadata["numwords"] = 0
metadata["numdocs"] = 0
metadata["numpostinglists"] = 0
metadata["sumdocwords"] = 0 #used for computing average document length
metadata["postingcount"] = 0
metadata["numgooddocs"] = 0
metadata["numbaddocs"] = 0
metadata["numnotextdocs"] = 0

buffer = ""
rawtext = ""
elements = []
#<DOC>, <DOCNO>, </DOCNO>, <TEXT>, </TEXT>, </DOC>
urllist = []
freqdict = {}
wordlist = []

######################################################################
##################open some files and create posting directory ######

print("generate_postings_paper.py starting...")

filehandles = {}

try:    ###open corpus file
    corpusfh = open(corpusfp, 'r', encoding = "utf8")
    filehandles["corpusfh"] = corpusfh

except:
    print("Unable to open corpus file " + corpusfp)
    quit()
try:    ###open lexicon file
    lexiconfh = open('lexicon.txt', 'w+')
    filehandles["lexiconfh"] = lexiconfh

except:
    print("Unable to open lexicon file")
    quit()
try:    ###open pagetable file
    pagetablefh = open('pagetable.txt', 'w+')
    filehandles["pagetablefh"] = pagetablefh

except:
    print("Unable to open pagetable file")
    quit()

try:
    notextdocsfh = open("notextdocs.txt", "w+")
    filehandles["notextdocsfh"] = notextdocsfh
except:
    print("Unable to open no text docs file")
    quit()

if (os.path.isdir(pwd+"\\postings")):
    try:    ###remove existing postings directory
        shutil.rmtree('postings')
        print('postings folder and all its contents deleted')
    except:
        print("unable to remove postings folder")

print('Creating new postings folder')
os.mkdir(pwd + '\\postings')


################################################################
#############main loop################################################

elements = [False, False, False, False, False, False]
buffer = ""

for l in filehandles["corpusfh"]:
#while (1):
    #l = filehandles["corpusfh"].readline()

    #if (len(l) == 0):
    #    break

    line = bytes(l, "utf-8").decode("ascii", "ignore")
    #line = bytes(l, "utf-8").decode("utf8", "ignore")

    rawtext = rawtext+line+"\n"

    if ("http" in line):
        urllist.append(line.strip())
        continue

    if ("<DOC>" in  line):
        if (elements[0]):
            metadata["numbaddocs"] = metadata["numbaddocs"] + 1
            elements = [False, False, False, False, False, False]

            update_data_structures(buffer, rawtext, urllist, lexicon, pagetable, postingsbuffer, metadata, filehandles)

            if ((len(postingsbuffer) > posting_buffer_limit) or (sys.getsizeof(postingsbuffer) > posting_buffer_size_limit)):
                print("Elapsed time: {0:.2f} minutes, lexicon size: {1:.2f} GB, pagetable size: {2:.2f} GB,".format((time.time()-tic)/60.0, sys.getsizeof(lexicon)/1000000, sys.getsizeof(pagetable)/1000000))
                write_postings_buffer(postingsbuffer, metadata, binary = binarypostingfiles)

            buffer = ""
            rawtext = ""
            urllist = []

            if (metadata["numdocs"]%doc_interval == 0):
                print(str(metadata["numdocs"]) + " documents processed, elapsed time: {0:.2f} minutes".format((time.time()-tic)/60.0))

            if (metadata["numdocs"] == doc_limit):
                break

            continue
        else:
            elements[0] = True
            continue

    if ("<DOCNO>" in line):
        elements[1] = True
        continue

    if ("</DOCNO>" in line):
        elements[2] = True
        continue

    if ("<TEXT>" in line):
        elements[3] = True
        continue

    if ("</TEXT>" in line):
        elements[4] = True
        continue

    if ("</DOC>" in line):
        elements = [False, False, False, False, False, False]
        metadata["numgooddocs"] = metadata["numgooddocs"] + 1

        update_data_structures(buffer, rawtext, urllist, lexicon, pagetable, postingsbuffer, metadata, filehandles)

        if ((len(postingsbuffer) > posting_buffer_limit) or (sys.getsizeof(postingsbuffer) > posting_buffer_size_limit)):
            print("Elapsed time: {0:.2f} minutes, lexicon size: {1:.2f} GB, pagetable size: {2:.2f} GB,".format((time.time()-tic)/60.0, sys.getsizeof(lexicon)/1000000, sys.getsizeof(pagetable)/1000000))
            write_postings_buffer(postingsbuffer, metadata, binary = binarypostingfiles)

        buffer = ""
        rawtext = ""
        urllist = []

        if (metadata["numdocs"]%doc_interval == 0):
            print(str(metadata["numdocs"]) + " documents processed, elapsed time: {0:.2f} minutes".format((time.time()-tic)/60.0))

        if (metadata["numdocs"] == doc_limit):
            break

        continue

    buffer = buffer + "\n" + line

#####################################################################
#########save any remaining postings to file########################

if (len(postingsbuffer) > 0):
    print("Writing remaining " + str(len(postingsbuffer)) + " postings to file")
    write_postings_buffer(postingsbuffer, metadata, binary = binarypostingfiles)

#####################################################################
#######write metadata to file #####################################

with open(cwd+"//postings//"+"postingmetadata.txt", 'w+') as metadatafile:
    metadatafile.write("n_posting_files " + str(metadata["numpostinglists"])+"\n") #write number of posting files
    metadatafile.write("n_postings " + str(metadata["postingcount"]) +"\n") #write total number of postings
    metadatafile.write("n_words " + str(metadata["numwords"]) + "\n" )
    metadatafile.write("n_docs " + str(metadata["numdocs"]) + "\n")
    metadatafile.write("avg_doc_length " + "{0:.4f}".format(metadata["sumdocwords"]/metadata["numdocs"]) + "\n") #document length in words
    metadatafile.write("n_good_docs " + str(metadata["numgooddocs"]) + "\n")
    metadatafile.write("n_bad_docs " + str(metadata["numbaddocs"]) + "\n")

###########################################################################
##########close file handles####################

for fh in filehandles.keys():
    filehandles[fh].close()

print("generate_postings_paper.py finished running. Elapsed time: {0:.2f} minutes".format((time.time()-tic)/60.0))
