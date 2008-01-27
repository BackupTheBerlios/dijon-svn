#include <iostream>
#include <cjkv-tokenizer.hh>

using namespace std;
using namespace cjkv;

int main() {
    tokenizer tknzr;
    vector<string> token_list;
    vector<string>::iterator token_iter;
    char text[] =
        "美女遊戲等你挑戰周蕙最新鈴搶先下載茄子醬耍可愛一流"
        "납치 여중생 공포에 떠는데'…경찰 200m 거리 25분만에 출동"
        "寛永通宝の一。京都方広寺の大仏をこわして1668年（寛文8）から鋳造した銅銭。"
        "ㄅㄆㄇㄈㄉㄊㄋㄌㄧㄨㄩ"
        "Giant Microwave Turns Plastic Back to Oil";
    string text_str = string(text);

    cout << "[Default]" << endl;
    token_list.clear();
    cout << "Ngram size: "  << tknzr.ngram_size << endl;
    tknzr.tokenize(text_str, token_list);
    cout << "Original string: " << text << endl;
    cout << "Tokenized result: ";
    for (token_iter = token_list.begin();
         token_iter != token_list.end(); token_iter++) {
        cout << "[" << *token_iter << "] ";
    }
    cout << endl << endl;

    cout << "[Trigram]" << endl;
    token_list.clear();
    tknzr.ngram_size = 3;
    cout << "Ngram size: "  << tknzr.ngram_size << endl;
    tknzr.tokenize(text_str, token_list);
    cout << "Original string: " << text << endl;
    cout << "Tokenized result: ";
    for (token_iter = token_list.begin();
         token_iter != token_list.end(); token_iter++) {
        cout << "[" << *token_iter << "] ";
    }
    cout << endl << endl;

    cout << "[Pentagram]" << endl;
    token_list.clear();
    tknzr.ngram_size = 5;
    cout << "Ngram size: "  << tknzr.ngram_size << endl;
    tknzr.tokenize(text_str, token_list);
    cout << "Original string: " << text << endl;
    cout << "Tokenized result: ";
    for (token_iter = token_list.begin();
         token_iter != token_list.end(); token_iter++) {
        cout << "[" << *token_iter << "] ";
    }
    cout << endl << endl;

    cout << "[Max token count]" << endl;
    token_list.clear();
    tknzr.max_token_count = 10;
    cout << "Max token count: " << tknzr.max_token_count << endl;
    tknzr.tokenize(text_str, token_list);
    cout << "Original string: " << text << endl;
    cout << "Tokenized result: ";
    for (token_iter = token_list.begin();
         token_iter != token_list.end(); token_iter++) {
        cout << "[" << *token_iter << "] ";
    }
    cout << endl << endl;

    cout << "[Unigram]" << endl;
    token_list.clear();
    tknzr.ngram_size = 1;
    tknzr.max_token_count = 0;
    tknzr.tokenize(text_str, token_list);
    cout << "Original string: " << text << endl;
    cout << "Tokenized result: ";
    for (token_iter = token_list.begin();
         token_iter != token_list.end(); token_iter++) {
        cout << "[" << *token_iter << "] ";
    }
    cout << endl << endl;

    cout << "[Split]" << endl;
    token_list.clear();
    tknzr.split(text_str, token_list);
    cout << "Original string: " << text << endl;
    cout << "Tokenized result: ";
    for (token_iter = token_list.begin();
         token_iter != token_list.end(); token_iter++) {
        cout << "[" << *token_iter << "] ";
    }
    cout << endl << endl;

    vector<unicode_char_t> uchar_list;
    vector<unicode_char_t>::iterator uchar_iter;

    cout << "[Split (unicode_char_t)]" << endl;
    tknzr.split(text_str, uchar_list);
    cout << "Original string: " << text << endl;
    cout << "Tokenized result: ";
    for (uchar_iter = uchar_list.begin();
         uchar_iter != uchar_list.end(); uchar_iter++) {
        cout << "[" << *uchar_iter << "] ";
    }
    cout << endl << endl;

    string cjkv_str = "這是CJKV字串";
    string pure_cjkv_str =
        "這個字串只含中日韓。"
        "コンピューターの機能を、音響・映像・作品制御などに利用する芸術の総称。"
        "납치 여중생 공포에 떠는데'…경찰 200m 거리 25분만에 출동"
        "ㄅㄆㄇㄈㄉㄊㄋㄌㄧㄨㄩ";
    cout << "[" << cjkv_str << "]" << " has CJKV characters? "
         << tknzr.has_cjkv(cjkv_str) << endl;
    cout << "[" << cjkv_str << "]" << " has CJKV characters only? "
         << tknzr.has_cjkv_only(cjkv_str) << endl;
    cout << "[" << pure_cjkv_str << "]" << " has CJKV characters? "
         << tknzr.has_cjkv(pure_cjkv_str) << endl;
    cout << "[" << pure_cjkv_str << "]" << " has CJKV characters only? "
         << tknzr.has_cjkv_only(pure_cjkv_str) << endl;
    cout << endl;
    return 0;
}
