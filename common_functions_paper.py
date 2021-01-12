import re
import nltk
nltk.download('stopwords')
nltk.download('words')
nltk.download('wordnet')
from nltk.corpus import stopwords
from nltk.stem import PorterStemmer
import pandas as pd

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

def write_query_words(input_file_path, querywordfilename):

    input_df = pd.read_csv(input_file_path, sep = "\t", names = ['qid', 'query'],)

    qwfh = open(querywordfilename, 'w+')

    for i in range(input_df.shape[0]):

        qwfh.write(str(input_df.iloc[i, 0]) + " ")

        words = get_words(input_df.iloc[i, 1])

        if (len(words) == 0):
            qwfh.write("\n")
        else:
            for j in range(len(words)-1):

                qwfh.write(words[j] + " ")

            qwfh.write(words[-1] + "\n")

    qwfh.close()
