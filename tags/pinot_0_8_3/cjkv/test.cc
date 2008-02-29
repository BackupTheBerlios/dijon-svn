/*
 *  Copyright 2007,2008 林永忠 Yung-Chung Lin
 *  Copyright 2008 Fabrice Colin
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <iostream>

#include "CJKVTokenizer.h"

using namespace std;
using namespace Dijon;

int main() {
    CJKVTokenizer tknzr;
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
    cout << "Ngram size: "  << tknzr.get_ngram_size() << endl;
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
    tknzr.set_ngram_size(3);
    cout << "Ngram size: "  << tknzr.get_ngram_size() << endl;
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
    tknzr.set_ngram_size(5);
    cout << "Ngram size: "  << tknzr.get_ngram_size() << endl;
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
    tknzr.set_max_token_count(10);
    cout << "Max token count: " << tknzr.get_max_token_count() << endl;
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
    tknzr.set_ngram_size(1);
    tknzr.set_max_token_count(0);
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

    string cjkv_str = "这个商店是买中国画儿的";
    cout << "[Tokenize]" << endl;
    token_list.clear();
    tknzr.set_ngram_size(2);
    cout << "Ngram size: "  << tknzr.get_ngram_size() << endl;
    tknzr.tokenize(cjkv_str, token_list);
    cout << "Original string: " << cjkv_str << endl;
    cout << "Tokenized result: ";
    for (token_iter = token_list.begin();
         token_iter != token_list.end(); token_iter++) {
        cout << "[" << *token_iter << "] ";
    }
    cout << endl << endl;

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
