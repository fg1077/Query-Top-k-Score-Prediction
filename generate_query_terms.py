import common_functions_paper

directory = "C:\\Users\\frank\\Desktop\\nyu\\FA20\\web search engines\\TREC-2019\\"

trainfn = "queries.doctrain.tsv"
devfn = "queries.docdev.tsv"
testfn = "msmarco-test2019-queries.tsv"

common_functions_paper.write_query_words(directory+trainfn, directory+"qw_train.txt")

common_functions_paper.write_query_words(directory+devfn, directory+"qw_dev.txt")

common_functions_paper.write_query_words(directory+testfn, directory+"qw_test.txt")
