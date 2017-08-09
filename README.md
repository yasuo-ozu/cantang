# cantang - たった600行の簡易インタープリタのモデル実装

## cantang とは
cantangは、1ファイル(約600行)のC言語コードで実装されたインタープリタです。実行できるコードはC言語のサブセットとなっています。

## 特徴
トークン解析、構文解析をかなり単純化しています。また構文木を生成せず、トークン列を直接実行します。
コンパイラをシンプルに実装するために速度を犠牲にしており、以下の理由により、処理に通常の処理系と比べると1000~10000倍の時間がかかります。

- コード上の同じ箇所を毎回構文解析を行うため。
- コード上の実行しない部分についても毎回構文解析を行うため。
- 再帰下降パーサにより式を処理しているため。

また、速度以外にも以下のような特徴が有ります。

- 型がない (すべてlong long型と同等のものとして処理する)

## 仕様
実際に動作するコードは
https://github.com/yasuo-ozu/cantang/tree/master/tests
で確認できます。

### 対応している機能

- 変数の宣言、初期化
- 各種演算子( 前置 , 後置 , 二項 , 三項演算子等 )
- 文字列
- print 文 ( 数値表示 ), puts 文 ( 文字列表示 )
- if-else, for, while, do-while, return, break, continue
- 多次元配列
- 関数宣言 , 引数 , 再帰関数
- struct

### 対応していない(C言語の)機能

- 入れ子になった構造体
- enum, union
- typedef
- ポインタへの足し算(バイト単位の計算になる)
- キャスト(型がないため)
- ファイルアクセス, 標準ライブラリの多くの機能
- and so on

## 仕組み

### 1. トークン解析
入力されたコードはまずトークン解析器にかけられます。トークン解析では、入力をトークン列に分解します。このとき、各種リテラル、記号、キーワード、それ以外の識別子を区別しフラグにセットします。キーワードを区別するためにキーワードリストを保持します。これは変数名としてキーワードを使用する等のエラーを検出するために有用です。また、記号についても2文字以上の記号によって構成される演算子を1つのものと認識するために記号列を保持しています。

なお、ここで作られたトークン列はそのまま実行に利用されるので、一度作成されたトークンが削除されることはありません。

### 2. 実行
cantangでは、構文解析と実行を同時に行っています。例えば、現在注目しているトークンが「for」であれば、即座にforを処理する部分に飛びます。繰り返し処理では、繰り返し処理の最初のトークンの位置が記憶されていて、繰り返し処理の終端にて条件に合致すれば記憶されていたトークン位置に現在注目しているトークンが戻されます。

if文などで条件が成立しない場合、if文の処理の続きをスキップする必要があります。この場合は、if文の内部の処理を「空実行」します。なぜこれが必要であるかというと、「if」というトークンを解釈している段階ではまだどこまでスキップすればよいか把握できないためです。

## コンパイルの方法
以下のようにmain.cをコンパイルするだけです。
```
> gcc main.c
```

以下のように実行することが出来ます。

```
> ./a.out -
int a = 1+2*3/4+5*(6+7-8);
print a;
27
```
