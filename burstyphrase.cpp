#include <algorithm>
#include <functional>
#include <fstream>
#include <locale>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "cmdline.h"
#include "sais.hxx"

using namespace std;

int getID(const string& word, unordered_map<string, int>& word2id){
  unordered_map<string, int>::const_iterator it = word2id.find(word);
  if (it == word2id.end()){
    int newID = (int)word2id.size();
    word2id[word] = newID;
    return newID;
  } else {
    return it->second;
  }
}

string getTerm(const vector<int>& T, const int beg, const int len,
               const vector<string>& id2word, const bool is_word){
  string term = "";
  for (int i = 0; i < len; ++i){
    if (beg + i >= (int)T.size()){
      break;
    }
    int c = T[beg + i];
    if (is_word){
      term += id2word[c] + " ";
    } else {
      term += id2word[c];
    }
  }
  if (is_word && term.length() > 0){
    term.erase(term.size()-1);
  }
  return term;
}

// encoding should be utf8
int readCorpus(const string text_file, vector<int>& T, vector<int>& D,
               unordered_map<string, int>& word2id, unordered_set<int>& wspace,
               unordered_set<int>& space, unordered_set<int>& punct,
               const bool is_word, const bool is_case_insensitive){
  cerr << "loading " << text_file << "... ";
  ifstream ifs(text_file.c_str());
  int docid = -1;
  string term;
  string word;
  char c;
  int utf8len = 0;
  while (ifs.get(c)){
    if (c == 0x02){
      while (c != 0x03) {
        ifs.get(c);
      }
      docid++;
      term = "";
      word = "";
      utf8len = 0;
      ifs.get(c);
    }
    if (c >> 5 == -2){
      word = c;
      utf8len = 2;
    } else if (c >> 4 == -2){
      word = c;
      utf8len = 3;
    } else if (c >> 3 == -2){
      word = c;
      utf8len = 4;
    } else if (c >> 2 == -2){
      word = c;
      utf8len = 5;
    } else if (c >> 1 == -2){
      word = c;
      utf8len = 6;
    } else if (c >> 1 == -1){
      //cerr << "invalid char ignored" << endl;
      term = "";
      word = "";
      utf8len = 0;
    } else if (c >= 0){
      word = c;
      utf8len = 1;
    } else {
      word += c;
    }
    --utf8len;
    if (utf8len == 0){
      if (is_word){
        wchar_t wc[10];
        mbstowcs(wc, word.c_str(), 10);
        if (iswalnum(wc[0])){
          if (is_case_insensitive){
            transform(word.begin(), word.end(), word.begin(), ::tolower);
          }
          term += word;
        } else {
          if (term != ""){
            T.push_back(getID(term, word2id));
            D.push_back(docid);
            term = "";
          }
          if (word == " " || word == "　"){
            space.insert(getID(word, word2id));
          } else if (iswcntrl(wc[0])){
            wspace.insert(getID(word, word2id));
            T.push_back(getID(word, word2id));
            D.push_back(docid);
          } else if (iswpunct(wc[0])){
            punct.insert(getID(word, word2id));
            T.push_back(getID(word, word2id));
            D.push_back(docid);
          }
        }
      } else {
        if (is_case_insensitive){
          transform(word.begin(), word.end(), word.begin(), ::tolower);
        }
        wchar_t wc[2];
        mbstowcs(wc, word.c_str(), 1);
        if (word == " " || word == "　"){
          space.insert(getID(word, word2id));
        } else if (iswspace(wc[0])){
          wspace.insert(getID(word, word2id));
        } else if (iswpunct(wc[0])){
          punct.insert(getID(word, word2id));
        }
        T.push_back(getID(word, word2id));
        D.push_back(docid);
      }
    }
  }
  if (term != ""){
    T.push_back(getID(term, word2id));
    D.push_back(docid);
  }
  ifs.close();
  docid++;
  cerr << "done." << endl;
  return docid;
}





/*
 * Compute burst context entropy (bcv)
 */
int calcBurstContextVariety(const int burst_num, unordered_map<int, unordered_set<int> >& context2pos){
  int num_remain = burst_num;
  unordered_map<int, unordered_set<int> > minset;
  unordered_map<int, unordered_set<int> > pos2context;
  unordered_map<int, set<int> > priority_dic;
  int max_num = 0;

  // Build priority dic for finding dominant contexts
  for (unordered_map<int, unordered_set<int> >::const_iterator it = context2pos.begin(); it != context2pos.end(); ++it){
    int context = it->first;
    int num = it->second.size();
    if (priority_dic.count(num) == 0){
      set<int> tmpset;
      priority_dic[num] = tmpset;
    }
    priority_dic[num].insert(context);
    if (max_num < num){
      max_num = num;
    }
    for (unordered_set<int>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2){
      int pos = *it2;
      if (pos2context.count(pos) == 0){
        unordered_set<int> tmpset;
        pos2context[pos] = tmpset;
      }
      pos2context[pos].insert(context);
    }
  }

  // Iteratively select contexts that best covers remaining posisions
  for (int i = max_num; i > 0; --i){
    while (priority_dic.count(i) > 0 && priority_dic[i].size() > 0){
      set<int>::iterator it = priority_dic[i].begin();
      unordered_set<int> thisposset = context2pos[*it];
      minset[*it] = thisposset;
      num_remain -= thisposset.size();
      if (num_remain <= 0){
        break;
      }
      //priority_dic[i].erase(it);
      for (unordered_set<int>::const_iterator it2 = thisposset.begin(); it2 != thisposset.end(); ++it2){
        int pos = *it2;
        for (unordered_set<int>::iterator it3 = pos2context[pos].begin(); it3 != pos2context[pos].end(); ++it3){
          //if (minset.count(*it3) == 0){
          priority_dic[context2pos[*it3].size()].erase(*it3);
          context2pos[*it3].erase(pos);
          priority_dic[context2pos[*it3].size()].insert(*it3);
          //}
        }
      }
    }
    if (num_remain <= 0){
      break;
    }
  }

  return (int)minset.size();
}





/*
 * Count the number of overlaps
 */
int countOverlaps(const unordered_map<int, unordered_set<int> >& this_overlap){
  int num = 0;
  for (unordered_map<int, unordered_set<int> >::const_iterator it = this_overlap.begin(); it != this_overlap.end(); ++it){
    num += it->second.size();
  }
  return num;
}





/*
 * Reduce overlaps that are canceled by the N-gram with cng_id
 */
unordered_map<int, unordered_set<int> > reduceOverlap(
    vector<int>& burst_nums,
    unordered_map<int, int>& num_covering,
    unordered_map<int, unordered_map<int, unordered_set<int> > >& overlap,
    unordered_map<int, unordered_map<int, unordered_set<int> > >& overlapped_by,
    unordered_map<int, set<int> >& priority_dic,
    //unordered_map<int, unordered_set<int> >& contphrasedic,
    const int cng_id){

  int num = 0;
  unordered_map<int, int> reduced_overlap;
  unordered_map<int, unordered_set<int> > this_overlap = overlap[cng_id];

  for (unordered_map<int, unordered_set<int> >::const_iterator it = this_overlap.begin(); it != this_overlap.end(); ++it){
    num += it->second.size();
    int bng_id = it->first;
    burst_nums[bng_id] -= it->second.size();
    if (burst_nums[bng_id] <= 0){ //burst cancel
      for (unordered_map<int, unordered_set<int> >::const_iterator it2 = overlapped_by[bng_id].begin();
           it2 != overlapped_by[bng_id].end(); ++it2){
        int cng_id2 = it2->first;
        int reduce_num = it2->second.size();
        if (reduced_overlap.count(cng_id2) == 0){
          reduced_overlap[cng_id2] = 0;
        }
        reduced_overlap[cng_id2] += reduce_num;
        overlap[cng_id2].erase(bng_id);
      }
      overlapped_by[bng_id].clear();
    }else{ //reduce the number of overlaps
      for (unordered_map<int, unordered_set<int> >::const_iterator it2 = overlapped_by[bng_id].begin();
           it2 != overlapped_by[bng_id].end();){
        int cng_id2 = it2->first;
        /*if (contphrasedic.count(cng_id) != 0 && contphrasedic[cng_id].count(cng_id2) != 0){
          ++it2;
          continue;
        }*/
        for (unordered_set<int>::const_iterator it3 = overlapped_by[bng_id][cng_id2].begin();
             it3 != overlapped_by[bng_id][cng_id2].end();){
          if (it->second.count(*it3) > 0){
            if (reduced_overlap.count(cng_id2) == 0){
              reduced_overlap[cng_id2] = 0;
            }
            reduced_overlap[cng_id2] += 1;
            overlap[cng_id2][bng_id].erase(*it3);
            overlapped_by[bng_id][cng_id2].erase(it3++);
          }else{
            ++it3;
          }
        }
        if (overlap[cng_id2][bng_id].size() == 0){
          overlap[cng_id2].erase(bng_id);
          overlapped_by[bng_id].erase(it2++);
        }else{
          ++it2;
        }
      }
    }
  }

  // Update priority dic
  for (unordered_map<int, int>::const_iterator it = reduced_overlap.begin(); it != reduced_overlap.end(); ++it){
    int cng_id2 = it->first;
    int reduce_num = it->second;
    priority_dic[num_covering[cng_id2]].erase(cng_id2);
    num_covering[cng_id2] -= reduce_num;
    if (num_covering[cng_id2] > 0){
      priority_dic[num_covering[cng_id2]].insert(cng_id2);
    }
  }

  return this_overlap;
}





/*
 * Find phrase containment relationships
 */
bool findInclusionRelations(
    vector<int>& burst_nums,
    unordered_map<int, int>& num_covering,
    unordered_map<int, unordered_map<int, unordered_set<int> > >& overlap,
    unordered_map<int, unordered_map<int, unordered_set<int> > >& overlapped_by,
    unordered_map<int, set<int> >& priority_dic,
    unordered_map<int, int>& bcvs, const vector<string>& ngrams,
    //unordered_map<int, unordered_set<int> >& contphrasedic,
    const int cng_id){

  // Find overlap phrases
  unordered_set<int> overlap_phrase;
  for (unordered_map<int, unordered_set<int> >::const_iterator it = overlap[cng_id].begin(); it != overlap[cng_id].end(); ++it){
    int bng_id = it->first;
    for (unordered_map<int, unordered_set<int> >::const_iterator it2 = overlapped_by[bng_id].begin();
         it2 != overlapped_by[bng_id].end(); ++it2){
      if(it2->first != cng_id){
        overlap_phrase.insert(it2->first);
      }
    }
  }

  // Find overlapping long phrases with higher bcv
  vector<int> overlap_longphrase;
  string longphrases = "";
  vector<int> tmp_overlap_longphrase;
  unordered_map<int, unordered_set<int> > canceled_overlap;
  for (unordered_set<int>::const_iterator it = overlap_phrase.begin(); it != overlap_phrase.end(); ++it){
    if (ngrams[*it].find(ngrams[cng_id]) != std::string::npos){ // overlapping long phrase
      if (bcvs[cng_id] <= bcvs[*it]){ // higher bcv
        overlap_longphrase.push_back(*it);
        longphrases += ngrams[*it] + "\t";
        int cng_id2 = *it;
        for (unordered_map<int, unordered_set<int> >::const_iterator it2 = overlap[cng_id2].begin();
             it2 != overlap[cng_id2].end(); ++it2){
          int bng_id = it2->first;
          if (overlap[cng_id].count(bng_id) > 0){
            for (unordered_set<int>::const_iterator it3 = it2->second.begin(); it3 != it2->second.end(); ++it3){
              if (overlap[cng_id][bng_id].count(*it3)){
                if (canceled_overlap.count(bng_id) == 0){
                  unordered_set<int> tmp_pos;
                  canceled_overlap[bng_id] = tmp_pos;
                }
                canceled_overlap[bng_id].insert(*it3);
              }
            }
          }
        }
      }else{
        tmp_overlap_longphrase.push_back(*it);
      }
    }
  }

  // Expand overlapping long phrases
  for (int i=0; i < (int)tmp_overlap_longphrase.size(); ++i){
    int cng_id2 = tmp_overlap_longphrase[i];
    if (longphrases.find(ngrams[cng_id2]) != std::string::npos){ // same group of long phrases
      overlap_longphrase.push_back(cng_id2);
      for (unordered_map<int, unordered_set<int> >::const_iterator it2 = overlap[cng_id2].begin();
           it2 != overlap[cng_id2].end(); ++it2){
        int bng_id = it2->first;
        if (overlap[cng_id].count(bng_id) > 0){
          for (unordered_set<int>::const_iterator it3 = it2->second.begin(); it3 != it2->second.end(); ++it3){
            if (overlap[cng_id][bng_id].count(*it3)){
              if (canceled_overlap.count(bng_id) == 0){
                unordered_set<int> tmp_pos;
                canceled_overlap[bng_id] = tmp_pos;
              }
              canceled_overlap[bng_id].insert(*it3);
            }
          }
        }
      }
    }
  }

  // Determine containment phrases
  vector<int> overlap_contphrase;
  int num_overlap = num_covering[cng_id];
  while(true){ //count num_overlap until it converges
    int tmp_num_overlap = num_covering[cng_id];

    //reduce num_overlap
    for (unordered_map<int, unordered_set<int> >::const_iterator it = canceled_overlap.begin();
         it != canceled_overlap.end(); ++it){
      int bng_id = it->first;
      if (overlapped_by[bng_id].count(cng_id) > 0){
        int tmp_num = 0;
        for (unordered_set<int>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2){
          if (overlapped_by[bng_id][cng_id].count(*it2) > 0){
            tmp_num += 1;
          }
        }
        if (burst_nums[bng_id] - tmp_num <= 0){ //burst cancel
          tmp_num_overlap -= overlapped_by[bng_id][cng_id].size();
        }else{
          tmp_num_overlap -= tmp_num;
        }
      }
    }

    if (num_overlap == tmp_num_overlap){ //converges
      /*for (int i = 0; i < (int)overlap_contphrase.size(); ++i){
        int cng_id2 = overlap_contphrase[i];
        if (contphrasedic.count(cng_id2) == 0){
          unordered_set<int> tmpset;
          contphrasedic[cng_id2] = tmpset;
        }
        contphrasedic[cng_id2].insert(cng_id);
      }*/
      break;
    }else{ //reconstruct overlap_contphrase
      num_overlap = tmp_num_overlap;
      vector<int>().swap(overlap_contphrase);
      canceled_overlap.clear();
      for (int i = 0; i < (int)overlap_longphrase.size(); ++i){
        int cng_id2 = overlap_longphrase[i];
        if (tmp_num_overlap <= num_covering[cng_id2]){ //overlap contphrase should cover more bursty N-grams
          overlap_contphrase.push_back(cng_id2);
          for (unordered_map<int, unordered_set<int> >::const_iterator it2 = overlap[cng_id2].begin();
               it2 != overlap[cng_id2].end(); ++it2){
            int bng_id = it2->first;
            if (overlapped_by[bng_id].count(cng_id) > 0){
              if (canceled_overlap.count(bng_id) == 0){
                unordered_set<int> tmp_pos;
                canceled_overlap[bng_id] = tmp_pos;
              }
              for (unordered_set<int>::const_iterator it3 = it2->second.begin(); it3 != it2->second.end(); ++it3){
                canceled_overlap[bng_id].insert(*it3);
              }
            }
          }
        }
      }
    }
  }

  // Reduce overlaps
  int reduce_num = 0;
  for (unordered_map<int, unordered_set<int> >::const_iterator it = canceled_overlap.begin();
       it != canceled_overlap.end(); ++it){
    int bng_id = it->first;
    if (overlapped_by[bng_id].count(cng_id) > 0){
      int this_reduce_num = 0;
      for (unordered_set<int>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2){
        if (overlapped_by[bng_id][cng_id].count(*it2) > 0){
          overlap[cng_id][bng_id].erase(*it2);
          overlapped_by[bng_id][cng_id].erase(*it2);
          this_reduce_num += 1;
        }
      }
      if (burst_nums[bng_id] - this_reduce_num <= 0){ //burst cancel
        reduce_num += this_reduce_num + overlapped_by[bng_id][cng_id].size();
        overlap[cng_id].erase(bng_id);
        overlapped_by[bng_id].erase(cng_id);
      } else {
        reduce_num += this_reduce_num;
        /*if (overlap[cng_id][bng_id].size() == 0){
          overlap[cng_id].erase(bng_id);
          overlapped_by[bng_id].erase(cng_id);
        }*/
      }
    }
  }
  priority_dic[num_covering[cng_id]].erase(cng_id);
  num_covering[cng_id] -= reduce_num;
  priority_dic[num_covering[cng_id]].insert(cng_id);

  bool is_contained = false;
  if (reduce_num > 0){
    is_contained = true;
  }
  return is_contained;
}





/*
 * Find minimum set of N-grams that cover surplus occurrences of bursty N-grams
 */
unordered_map<int, unordered_map<int, unordered_set<int> > > setCoverBurst(
    vector<int>& burst_nums,
    unordered_map<int, unordered_map<int, unordered_set<int> > >& overlap,
    unordered_map<int, unordered_map<int, unordered_set<int> > >& overlapped_by,
    unordered_map<int, int>& bcvs, const vector<string>& ngrams){

  unordered_map<int, unordered_map<int, unordered_set<int> > > minset;
  unordered_map<int, set<int> > priority_dic;
  //unordered_map<int, unordered_set<int> > contphrasedic;
  unordered_map<int, int> num_covering;
  int max_num = 0;

  // Order by the number of overlaps
  for (unordered_map<int, unordered_map<int, unordered_set<int> > >::const_iterator it = overlap.begin();
       it != overlap.end(); ++it){
    //if (it->second.size() == 0){
      //continue;
    //}
    int cng_id = it->first;
    int num = countOverlaps(it->second);
    num_covering[cng_id] = num;
    if (priority_dic.count(num) == 0){
      set<int> tmpset;
      priority_dic[num] = tmpset;
    }
    priority_dic[num].insert(cng_id);
    if (max_num < num){
      max_num = num;
    }
  }

  // Iteratively select N-grams that best covers remaining occurrences of bursty N-grams
  for (int i = max_num; i > 0; --i){
    while (priority_dic.count(i) > 0 && priority_dic[i].size() > 0){
      set<int>::iterator it = priority_dic[i].begin();
      int cng_id = *it;
      bool is_contained = findInclusionRelations(burst_nums, num_covering, overlap, overlapped_by,
                                                priority_dic, bcvs, ngrams, cng_id);
      if (!is_contained){ //if this N-gram is not contained by any longer N-gram
        unordered_map<int, unordered_set<int> > this_overlap = reduceOverlap(burst_nums, num_covering,
                                                overlap, overlapped_by, priority_dic, cng_id);
        minset[cng_id] = this_overlap;
      }
    }
  }

  return minset;
}





/*
 * Main
 */
int main(int argc, char* argv[]){
  /*
   * Arguments
   */
  cmdline::parser p;
  p.add<string>("target", 'T', "target corpus");
  p.add<string>("reference", 'R', "reference corpus");
  p.add<string>("locale", 'L', "locale for language", false, "en_US.UTF8");
  p.add("wordmode", 'w', "word mode");
  p.add("caseinsensitivemode", 'c', "case insensitive mode");
  p.add("length", 'l', "max length for valid N-grams", false, 140);
  p.add("absdf", 'a', "min absolute df for valid N-grams", false, 2);
  p.add("reldf", 'r', "min relative df for valid N-grams", false, 1.5);
  p.add("zscore", 'z', "min z-score for bursty N-grams", false, 10.0);
  p.add("smoothing", 's', "smoothing value", false, 1.0);
  p.add("variety", 'v', "min burst context variety for candidate N-grams", false, 2);

  if (!p.parse(argc, argv)){
    cerr << p.error() << endl << p.usage() << endl;
    return -1;
  }

  if (p.rest().size() > 0){
    cerr << p.usage() << endl;
    return -1;
  }

  const string target_file = p.get<string>("target");
  const string reference_file = p.get<string>("reference");
  const string lang_locale = p.get<string>("locale");
  setlocale(LC_ALL, lang_locale.c_str());
  const bool is_word = p.exist("wordmode");
  const bool is_case_insensitive = p.exist("caseinsensitivemode");
  const int max_length = p.get<int>("length");
  const int df_threshold = p.get<int>("absdf");
  const double rel_df_threshold = p.get<double>("reldf");
  const double zscore_threshold = p.get<double>("zscore");
  const double smoothing = p.get<double>("smoothing");
  const double bcv_threshold = p.get<int>("variety");

  cerr << "locale: " << lang_locale << endl;
  if (is_word){
    cerr << "word mode: on" << endl;
  } else {
    cerr << "word mode: off" << endl;
  }
  if (is_case_insensitive){
    cerr << "case insensitive mode: on" << endl;
  } else {
    cerr << "case insensitive mode: off" << endl;
  }
  cerr << "max length: " << max_length << endl;
  cerr << "absolute df threshold: " << df_threshold << endl;
  cerr << "relative df threshold: " << rel_df_threshold << endl;
  cerr << "z-score threshold: " << zscore_threshold << endl;
  cerr << "smoothing value: " << smoothing << endl;
  cerr << "bcv threshold: " << bcv_threshold << endl << endl;

  /*
   * Variables to store texts
   */
  vector<int> Tt;
  vector<int> Dt;
  vector<int> Tr;
  vector<int> Dr;
  unordered_map<string, int> word2id;
  unordered_set<int> wspace;
  unordered_set<int> space;
  unordered_set<int> punct;

  /*
   * Reading target text corpus
   */
  int dnumt = readCorpus(target_file, Tt, Dt, word2id, wspace, space, punct, is_word, is_case_insensitive);
  int nt = Tt.size();
  vector<int> SAt(nt);
  int alphat = (int)word2id.size();
  cerr << endl << "document num: " << dnumt << endl;
  if (is_word){
    cerr << "word num: " << nt << endl;
    cerr << "unique word num: " << alphat << endl;
  } else {
    cerr << "character num: " << nt << endl;
    cerr << "unique character num: " << alphat << endl << endl;
  }
  cerr << "constructing suffix array... ";
  if (saisxx(Tt.begin(), SAt.begin(), nt, alphat) == -1){
    return -1;
  }
  cerr << "done." << endl;

  /*
   * Reading reference text corpus
   */
  int dnumr = readCorpus(reference_file, Tr, Dr, word2id, wspace, space, punct, is_word, is_case_insensitive);
  int nr = Tr.size();
  double nrate = (double)(nt)/(nr);
  double dnumrate = (double)(dnumt)/(dnumr);
  vector<int> SAr(nr);
  int alpha = (int)word2id.size();
  cerr << endl << "document num: " << dnumr << endl;
  if (is_word){
    cerr << "word num: " << nr << endl;
    cerr << "unique word num (+target): " << alpha << endl;
  } else {
    cerr << "character num: " << nr << endl;
    cerr << "unique character num (+target): " << alpha << endl << endl;
  }
  cerr << "constructing suffix array... ";
  if (saisxx(Tr.begin(), SAr.begin(), nr, alpha) == -1){
    return -1;
  }
  cerr << "done." << endl;





  //for (unordered_set<int>::const_iterator it = wspace.begin();
    //it != wspace.end(); ++it){
    //space.insert(*it);
  //}

  // Inverted indices from id to word/character
  vector<string> id2word(word2id.size());
  for (unordered_map<string, int>::const_iterator it = word2id.begin(); it != word2id.end(); ++it){
    id2word[it->second] = it->first;
  }

  vector<int> cf(max_length, 0);
  vector<int> cfr(max_length, 0);
  vector<unordered_map<int, int> > df(max_length);
  vector<unordered_map<int, int> > dfr(max_length);
  vector<unordered_map<int, int> > prefix(max_length);
  vector<unordered_map<int, int> > postfix(max_length);
  vector<int> begin(max_length, 0);
  vector<int> end(max_length, 0);
  vector<vector<int> > pos(max_length);
  vector<string> ngrams;
  vector<double> ng_mu;
  vector<double> ng_zscore;
  unordered_map<string, int> ngram2id;
  map<int, vector<pair<int, int> > > pos_to_ngrams; // position -> vector<bursty N-gram, start position>
  vector<int> burst_nums; // number of surplus occurrences of bursty N-grams





  /*
   * Enumerating bursty N-grams,
   * satisfying abs df, rel df, and zscore thresholds
   */
  cerr << "enumerating bursty N-grams... ";
  for (int i = 0; i < nt; ++i){
    if (space.count(Tt[SAt[i]]) > 0 || wspace.count(Tt[SAt[i]]) > 0){
      continue;
    }
    bool is_diff = false;
    int diff_at = max_length;
    int fill_at = -1;
    for (int k = 0; k < max_length; ++k){
      if (SAt[i]+k >= nt || wspace.count(Tt[SAt[i]+k]) > 0){
        break;
      }
      if (is_diff || i+1 == nt || Tt[SAt[i]+k] != Tt[SAt[i+1]+k]){
        if (postfix[k].size() == 0){
          break;
        }
        if (!is_diff){
          is_diff = true;
          diff_at = k;
        }
      }

      //cf
      cf[k] += 1;

      //df
      if (df[k].count(Dt[SAt[i]]) == 0){
        df[k][Dt[SAt[i]]] = 0;
      }
      df[k][Dt[SAt[i]]] += 1;

      //pos
      pos[k].push_back(SAt[i]);

      //prefix
      /*int j = 1;
      while (SAt[i]-j >= 0 && space.count(Tt[SAt[i]-j]) > 0){
        ++j;
      }
      if (SAt[i]-j < 0 || wspace.count(Tt[SAt[i]-j]) > 0){
        if (prefix[k].count(-1) == 0){
          prefix[k][-1] = 0;
        }
        prefix[k][-1] += 1;
      } else {
        if (prefix[k].count(Tt[SAt[i]-j]) == 0){
          prefix[k][Tt[SAt[i]-j]] = 0;
        }
        prefix[k][Tt[SAt[i]-j]] += 1;
      }*/

      //postfix
      int j = 1;
      while (SAt[i]+k+j < nt && space.count(Tt[SAt[i]+k+j]) > 0){
        ++j;
      }
      if (SAt[i]+k+j == nt || wspace.count(Tt[SAt[i]+k+j]) > 0){
        if (postfix[k].count(-1) == 0){
          postfix[k][-1] = 0;
        }
        postfix[k][-1] += 1;
      } else {
        if (postfix[k].count(Tt[SAt[i]+k+j]) == 0){
          postfix[k][Tt[SAt[i]+k+j]] = 0;
        }
        postfix[k][Tt[SAt[i]+k+j]] += 1;
      }

      //term end
      if (is_diff){
        if ((int)df[k].size() >= df_threshold && space.count(Tt[SAt[i]+k]) == 0){

          //df and cf from reference text corpus
          for (int l = 0; l < k+1; ++l){
            if (cfr[l] == 0){
              if (fill_at == -1){
                fill_at = l;
              }
              int ir = end[l];
              if (l > 0 && l > fill_at){
                ir = begin[l-1];
                if (ir >= end[l-1]){
                  break;
                }
              }
              while (ir < nr && Tr[SAr[ir]+l] < Tt[SAt[i]+l]){
                ir++;
              }
              begin[l] = ir;
              while (ir < nr && Tr[SAr[ir]+l] == Tt[SAt[i]+l] && (l == 0 || ir < end[l-1])){
                //cf
                cfr[l] += 1;
                //df
                if (dfr[l].count(Dr[SAr[ir]]) == 0){
                  dfr[l][Dr[SAr[ir]]] = 0;
                }
                dfr[l][Dr[SAr[ir]]] += 1;
                ir++;
              }
              end[l] = ir;
            }
          }

          //left context
          /*double lent = 0.0;
          int thistf = 0;
          for (unordered_map<int, int>::const_iterator it = prefix[k].begin();
               it != prefix[k].end(); ++it){
            if (wspace.count(it->first) > 0 || it->first == -1){
              thistf += it->second;
              double prob = (double)1/cf[k];
              lent -= it->second*prob*log2(prob);
            } else {
              thistf++;
              double prob = (double)it->second/cf[k];
              lent -= prob*log2(prob);
            }
          }
          int lvar = thistf;*/

          //right context
          /*double rent = 0.0;
          thistf = 0;
          for (unordered_map<int, int>::const_iterator it = postfix[k].begin();
               it != postfix[k].end(); ++it){
            if (wspace.count(it->first) > 0 || it->first == -1){
              thistf += it->second;
              double prob = (double)1/cf[k];
              rent -= it->second*prob*log2(prob);
            } else {
              thistf++;
              double prob = (double)it->second/cf[k];
              rent -= prob*log2(prob);
            }
          }
          int rvar = thistf;*/

          double rel_df = ((double)df[k].size()/(dfr[k].size()+smoothing)) / dnumrate;
          double mu = (dfr[k].size() * dnumrate) + smoothing;
          double zscore = ((double)df[k].size() - mu) / sqrt(mu);

          //bursty N-grams
          if (rel_df >= rel_df_threshold && zscore >= zscore_threshold){
            string term = getTerm(Tt, SAt[i], k+1, id2word, is_word);
            ngrams.push_back(term);
            ng_zscore.push_back(zscore);
            ng_mu.push_back(mu);

            //term frequency of burst should be covered
            double cf_mu = cfr[k] * nrate + smoothing;
            int normal_freq = int(cf_mu + zscore_threshold*sqrt(cf_mu));
            int burst_freq = cf[k] - normal_freq;
            burst_nums.push_back(burst_freq);

            int idx = (int)ngrams.size()-1;
            ngram2id[term] = idx;

            //occurrence positions
            for (int l = 0; l < (int)pos[k].size(); ++l){
              for (int m = 0; m < k+1; ++m){
                if (pos_to_ngrams.count(pos[k][l]+m) == 0){
                  pos_to_ngrams[pos[k][l]+m] = vector<pair<int, int> >();
                }
                pos_to_ngrams[pos[k][l]+m].push_back(pair<int, int>(idx, pos[k][l]));
              }
            }
          }
        }
      }
    }
    for (int k = diff_at; k < max_length; ++k){
      if (postfix[k].size() == 0){
        break;
      }
      cf[k] = 0;
      cfr[k] = 0;
      df[k].clear();
      dfr[k].clear();
      //prefix[k].clear();
      postfix[k].clear();
      vector<int>().swap(pos[k]);
    }
  }
  for (int k = 0; k < max_length; ++k){
    begin[k] = 0;
    end[k] = 0;
  }
  cerr << "done." << endl;





  /*
   * Enumerating candidate N-grams,
   * satisfying abs df, rel df, and bav thresholds, and
   * covering bursty N-grams
   */
  vector<int> cf_covering(max_length, 0); // to find overlaps
  unordered_map<int, unordered_map<int, unordered_set<int> > > overlap;
  unordered_map<int, unordered_map<int, unordered_set<int> > > overlapped_by;
  unordered_map<int, int> bcvs;
  for (int i = 0; i < (int)ngrams.size(); ++i){
    unordered_map<int, unordered_set<int> > this_overlapped_by;
    overlapped_by[i] = this_overlapped_by;
  }

  cerr << "enumerating candidate N-grams... ";
  for (int i = 0; i < nt; ++i){
    if (space.count(Tt[SAt[i]]) > 0 || wspace.count(Tt[SAt[i]]) > 0){
      continue;
    }
    bool is_diff = false;
    bool is_covering = false;
    int diff_at = max_length;
    int fill_at = -1;
    for (int k = 0; k < max_length; ++k){
      if (SAt[i]+k >= nt || wspace.count(Tt[SAt[i]+k]) > 0){
        break;
      }
      if (is_diff || i+1 == nt || Tt[SAt[i]+k] != Tt[SAt[i+1]+k]){
        if (postfix[k].size() == 0){
          break;
        }
        if (!is_diff){
          is_diff = true;
          diff_at = k;
        }
      }

      //covering bursty N-gram
      if (pos_to_ngrams.count(SAt[i]+k) > 0){
        is_covering = true;
      }

      //cf
      cf[k] += 1;
      if (is_covering){
        cf_covering[k] += 1;
      }

      //df
      if (df[k].count(Dt[SAt[i]]) == 0){
        df[k][Dt[SAt[i]]] = 0;
      }
      df[k][Dt[SAt[i]]] += 1;

      //pos
      pos[k].push_back(SAt[i]);

      //prefix
      int j = 1;
      while (SAt[i]-j >= 0 && space.count(Tt[SAt[i]-j]) > 0){
        ++j;
      }
      if (SAt[i]-j < 0 || wspace.count(Tt[SAt[i]-j]) > 0){
        if (prefix[k].count(-1) == 0){
          prefix[k][-1] = 0;
        }
        prefix[k][-1] += 1;
      } else {
        if (prefix[k].count(Tt[SAt[i]-j]) == 0){
          prefix[k][Tt[SAt[i]-j]] = 0;
        }
        prefix[k][Tt[SAt[i]-j]] += 1;
      }

      //postfix
      j = 1;
      while (SAt[i]+k+j < nt && space.count(Tt[SAt[i]+k+j]) > 0){
        ++j;
      }
      if (SAt[i]+k+j == nt || wspace.count(Tt[SAt[i]+k+j]) > 0){
        if (postfix[k].count(-1) == 0){
          postfix[k][-1] = 0;
        }
        postfix[k][-1] += 1;
      } else {
        if (postfix[k].count(Tt[SAt[i]+k+j]) == 0){
          postfix[k][Tt[SAt[i]+k+j]] = 0;
        }
        postfix[k][Tt[SAt[i]+k+j]] += 1;
      }

      //term end
      if (is_diff){
        if ((int)df[k].size() >= df_threshold && ((int)prefix[k].size() > 1 || prefix[k].count(-1) > 0) &&
            ((int)postfix[k].size() > 1 || postfix[k].count(-1) > 0) && space.count(Tt[SAt[i]+k]) == 0){

          //df and cf from reference text corpus
          for (int l = 0; l < k+1; ++l){
            if (cfr[l] == 0){
              if (fill_at == -1){
                fill_at = l;
              }
              int ir = end[l];
              if (l > 0 && l > fill_at){
                ir = begin[l-1];
                if (ir >= end[l-1]){
                  break;
                }
              }
              while (ir < nr && Tr[SAr[ir]+l] < Tt[SAt[i]+l]){
                ir++;
              }
              begin[l] = ir;
              while (ir < nr && Tr[SAr[ir]+l] == Tt[SAt[i]+l] && (l == 0 || ir < end[l-1])){
                //cf
                cfr[l] += 1;
                //df
                if (dfr[l].count(Dr[SAr[ir]]) == 0){
                  dfr[l][Dr[SAr[ir]]] = 0;
                }
                dfr[l][Dr[SAr[ir]]] += 1;
                ir++;
              }
              end[l] = ir;
            }
          }

          double rel_df = ((double)df[k].size()/(dfr[k].size()+smoothing)) / dnumrate;
          double mu = (dfr[k].size() * dnumrate) + smoothing;
          double zscore = ((double)df[k].size() - mu) / sqrt(mu);

          //bursty N-grams
          if (rel_df >= rel_df_threshold && cf_covering[k] > 0){

            //burst context entropy (bcv)
            int mean_cf = (int)(cfr[k] * nrate) + smoothing;
            int additional_cf = cf[k] - mean_cf;
            unordered_map<int, unordered_set<int> > context2pos;
            int ws = -1; //negative number for wspace characters
            for (int l = 0; l < (int)pos[k].size(); ++l){
              //prefix
              int j = 1;
              while (pos[k][l]-j >= 0 && space.count(Tt[pos[k][l]-j]) > 0){
                ++j;
              }
              if (pos[k][l]-j < 0 || wspace.count(Tt[pos[k][l]-j]) > 0){
                unordered_set<int> tmppos;
                tmppos.insert(pos[k][l]);
                context2pos[ws--] = tmppos;
              } else {
                if (context2pos.count(Tt[pos[k][l]-j]) == 0){
                  unordered_set<int> tmppos;
                  context2pos[Tt[pos[k][l]-j]] = tmppos;
                }
                context2pos[Tt[pos[k][l]-j]].insert(pos[k][l]);
              }
              //postfix
              j = 1;
              while (pos[k][l]+k+j < nt && space.count(Tt[pos[k][l]+k+j]) > 0){
                ++j;
              }
              if (pos[k][l]+k+j == nt || wspace.count(Tt[pos[k][l]+k+j]) > 0){
                unordered_set<int> tmppos;
                tmppos.insert(pos[k][l]);
                context2pos[ws--] = tmppos;
              } else {
                if (context2pos.count(Tt[pos[k][l]+k+j]+alpha) == 0){
                  unordered_set<int> tmppos;
                  context2pos[Tt[pos[k][l]+k+j]+alpha] = tmppos;
                }
                context2pos[Tt[pos[k][l]+k+j]+alpha].insert(pos[k][l]);
              }
            }
            int bcv = calcBurstContextVariety(additional_cf, context2pos);

            if (bcv >= bcv_threshold) {
              string term = getTerm(Tt, SAt[i], k+1, id2word, is_word);
              int idx;
              if (ngram2id.count(term) > 0){
                idx = ngram2id[term];
              }else{
                ngrams.push_back(term);
                ng_mu.push_back(mu);
                ng_zscore.push_back(zscore);
                idx = ngrams.size()-1;
              }
              bcvs[idx] = bcv;

              //covered posisions
              unordered_map<int, unordered_set<int> > this_overlap;
              for (int l = 0; l < (int)pos[k].size(); ++l){
                for (int m = 0; m < k+1; ++m){
                  if (pos_to_ngrams.count(pos[k][l]+m) > 0){
                    for (int n = 0; n < (int)pos_to_ngrams[pos[k][l]+m].size(); ++n){
                      int idy = pos_to_ngrams[pos[k][l]+m][n].first;
                      int startpos = pos_to_ngrams[pos[k][l]+m][n].second;
                      if (this_overlap.count(idy) == 0){
                        unordered_set<int> this_overlap_pos;
                        this_overlap[idy] = this_overlap_pos;
                      }
                      this_overlap[idy].insert(startpos);
                    }
                  }
                }
              }
              for (unordered_map<int, unordered_set<int> >::const_iterator it = this_overlap.begin();
                   it != this_overlap.end();++it){
                overlapped_by[it->first][idx] = it->second;
              }
              overlap[idx] = this_overlap;
            }
          }
        }
      }
    }
    for (int k = diff_at; k < max_length; ++k){
      if (postfix[k].size() == 0){
        break;
      }
      cf[k] = 0;
      cf_covering[k] = 0;
      cfr[k] = 0;
      df[k].clear();
      dfr[k].clear();
      prefix[k].clear();
      postfix[k].clear();
      vector<int>().swap(pos[k]);
    }
  }
  cerr << "done." << endl;





  /*
   * Extended Set Cover Problem
   */
  cerr << "solving set cover problem... ";
  unordered_map<int, unordered_map<int, unordered_set<int> > > minset = setCoverBurst(
      burst_nums, overlap, overlapped_by, bcvs, ngrams);
  cerr << "done." << endl;




  /*
   * Output
   */
  cerr << "outputting bursty phrases... ";
  vector<pair<int, double> > sorted_zscores;
  for (unordered_map<int, unordered_map<int, unordered_set<int> > >::const_iterator it = minset.begin();
       it != minset.end(); ++it){
    double max_zscore = 0.0;
    double limit_zscore = ng_zscore[it->first];
    for (unordered_map<int, unordered_set<int> >::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2){
      //df recompute
      unordered_set<int> re_df;
      for (unordered_set<int>::const_iterator it3 = it2->second.begin(); it3 != it2->second.end(); ++it3){
        re_df.insert(Dt[*it3]);
      }
      //z-score recompute
      double zscore = ((double)re_df.size() - ng_mu[it2->first]) / sqrt(ng_mu[it2->first]);
      if (max_zscore < zscore){
        max_zscore = zscore;
        if (max_zscore >= limit_zscore){
          max_zscore = limit_zscore;
          break;
        }
      }
    }
    sorted_zscores.push_back(pair<int, double>(it->first, max_zscore));
  }

  sort(sorted_zscores.begin(), sorted_zscores.end(), [](const pair<int, double> &left, const pair<int, double> &right){
    if (left.second != right.second){
      return left.second > right.second;
    }else{
      return left.first > right.first;
    }
  });

  for (int i = 0; i < (int)sorted_zscores.size(); ++i){
    pair<int, double> p = sorted_zscores[i];
    cout << ngrams[p.first] << "\t" << p.second << endl;
  }
  cerr << "done." << endl;
  return 0;
}
