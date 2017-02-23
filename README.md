# easy-nyaaan
卒論発表中にゃーんって呟きたくない?
呟きたいよね?

## makeあれこれ
* C++11くらいでどうぞ
* libcurl,liboauth使ってる
* boostも使ってる

## 実行あれこれ
* sudo ./a.out startで開始
* sudo ./a.out keytestでデバイスからの入力をテストできる
* sudo ./a.out devicesでデバイス名検索

## 設定あれこれ
main.confに
* TwitterAPIのいろいろを書く
* 入力デバイス名を調べて書く
* 入力テストをしてkey="呟きたい内容"って書く(同時に複数の信号が出る場合は他のボタンを押したときにはない特徴的なキーを選択する)
