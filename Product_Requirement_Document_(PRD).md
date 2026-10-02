Product Requirement Document (PRD)

Gen Z QA Dataset Generator

Versi: 1.0.0
Status: Draft / Ready for Development
Bahasa Utama: Bahasa Indonesia
Gaya Bahasa: Gen Z Indonesia / informal / conversational
Target: Dataset QA untuk training model AI dari nol
Format Dataset: JSONL / JSON / Parquet
Platform Target: Google Colab, Linux, Termux, dan local Python environment

---

1. Ringkasan Eksekutif

1.1 Nama Produk

Gen Z QA Dataset Generator

Sebuah pipeline untuk membuat dataset Question Answering (QA) dalam Bahasa Indonesia dengan gaya komunikasi Gen Z yang natural, santai, conversational, dan tetap informatif.

Dataset ditujukan untuk melatih model AI agar mampu:

- memahami pertanyaan formal maupun slang;
- memahami typo dan singkatan;
- memahami bahasa chat;
- menjawab dengan gaya natural;
- mempertahankan konteks percakapan;
- menjelaskan konsep teknis dengan bahasa sederhana;
- melakukan troubleshooting;
- menghasilkan contoh kode;
- mengubah bahasa formal menjadi bahasa yang lebih santai;
- memahami variasi ekspresi pengguna Gen Z.

---

2. Problem Statement

Dataset QA biasa sering terlalu formal.

Contoh:

User:
Apa yang dimaksud dengan overfitting?

Assistant:
Overfitting adalah kondisi ketika model machine learning terlalu menyesuaikan diri terhadap data training...

Secara teknis jawabannya benar, tetapi model yang dilatih menggunakan dataset seperti ini belum tentu terbiasa dengan pertanyaan:

"overfitting tuh apaan sih?"

"bro overfit itu kenapa ya?"

"ini model gw kok train bagus tapi val jelek?"

"anjir loss gw naik terus, itu kenapa?"

"gimana biar model gak overfit?"

"jelasin overfitting tapi jangan ribet pls"

Model perlu belajar bahwa semua pertanyaan tersebut dapat memiliki intent yang sama atau sangat berdekatan.

Karena itu diperlukan dataset yang memiliki variasi bahasa tinggi.

---

3. Tujuan Produk

3.1 Tujuan Utama

Membangun pipeline yang mampu menghasilkan dataset QA berkualitas tinggi dengan karakteristik:

Natural
+
Variatif
+
Conversational
+
Gen Z
+
Informative
+
Context-aware
+
Technically Correct

3.2 Tujuan Model yang Dilatih

Model yang menggunakan dataset ini diharapkan mampu memahami:

formal
informal
slang
typo
singkatan
campuran bahasa
chat style
pertanyaan pendek
pertanyaan panjang
multi-turn conversation
pertanyaan teknis
pertanyaan ambigu

---

4. Non-Goals

Produk ini tidak dimaksudkan untuk:

- membuat konten berbahaya;
- menghasilkan spam otomatis;
- membuat identitas palsu;
- meniru individu tertentu;
- menghasilkan dataset yang seluruhnya menggunakan slang hingga sulit dipahami;
- mengganti factual correctness dengan sekadar gaya bahasa.

Prinsip utama:

«Gaya Gen Z boleh santai, tetapi jawaban tetap harus benar.»

---

5. Target Pengguna

5.1 Primary User

Developer atau researcher yang sedang membangun model bahasa dari nol.

Contohnya:

AI researcher
ML engineer
NLP developer
student
indie developer
language-model developer

5.2 Secondary User

Developer yang ingin membuat chatbot khusus:

coding assistant
study assistant
documentation assistant
developer assistant
community chatbot
education chatbot

---

6. Prinsip Dataset

Dataset harus memenuhi lima prinsip utama.

6.1 Natural

Pertanyaan harus terdengar seperti manusia benar-benar sedang chatting.

Contoh:

"bro ini error apaan?"

"cara benerinnya gimana dah?"

"kok bisa gitu?"

"bedanya var sama let apaan?"

"jelasin gampang dong"

6.2 Diverse

Satu intent harus memiliki banyak variasi.

Contoh intent:

Apa itu Python?

Variasi:

"Python itu apaan?"

"python tuh bahasa apa sih?"

"bro python dipake buat apa?"

"Python gunanya buat apa?"

"jelasin Python dong"

"Python cocok buat pemula gak?"

"gw baru belajar coding, Python tuh apaan?"

6.3 Correct

Slang tidak boleh mengubah kebenaran teknis.

6.4 Contextual

Dataset harus mendukung konteks percakapan.

Contoh:

User:
Python itu apa?

Assistant:
Python adalah bahasa pemrograman...

User:
terus kenapa banyak yang pake?

Assistant:
Karena Python relatif mudah dipelajari...

6.5 Balanced

Dataset tidak boleh hanya berisi slang.

Distribusi target:

Formal            15%
Semi-formal       15%
Casual            20%
Gen Z             30%
Very casual       10%
Mixed language    10%

---

7. Definisi "Bahasa Gen Z"

Dataset menggunakan karakteristik bahasa yang umum digunakan dalam percakapan digital Indonesia.

Contoh pola:

bro
bang
bestie
guys
gw
gue
aku
lu
lo
kamu
kalo
gak
ga
nggak
kok
nih
sih
dong
lah
deh
wkwk
lol
btw
btw
imo
fr
literally
relate
valid
cringe
spill
random
flex
vibes
sat-set
gas
skip
mantap

Namun sistem tidak boleh memaksakan slang pada setiap kalimat.

Contoh buruk:

"Bro literally Python tuh vibes-nya gas banget fr..."

Contoh lebih natural:

"Bro, Python itu sebenarnya buat apa sih?"

"Kalau buat pemula, Python enak banget dipelajari karena sintaksnya relatif simpel."

---

8. Arsitektur Pipeline

Pipeline utama:

SOURCE
  |
  v
DOCUMENT INGESTION
  |
  v
CONTENT ANALYSIS
  |
  v
QUESTION GENERATION
  |
  v
GEN Z PARAPHRASING
  |
  v
ANSWER GENERATION
  |
  v
CONTEXT GENERATION
  |
  v
QUALITY VALIDATION
  |
  v
DUPLICATE FILTER
  |
  v
SAFETY FILTER
  |
  v
DATA BALANCING
  |
  v
TRAIN / VALIDATION / TEST
  |
  v
EXPORT

---

9. Input Dataset

Input dapat berasal dari:

Markdown
TXT
JSON
JSONL
CSV
HTML
Documentation
API documentation
source code
knowledge base

Contoh Markdown:

# Python

Python adalah bahasa pemrograman tingkat tinggi.

Python banyak digunakan untuk:
- web development
- automation
- data science
- machine learning

Pipeline membaca isi tersebut lalu menghasilkan QA.

---

10. Struktur Data Utama

Setiap sample minimal memiliki:

{
  "id": "qa_000001",
  "category": "definition",
  "topic": "python",
  "difficulty": "easy",
  "style": "gen_z",
  "question": "Python tuh apaan sih?",
  "answer": "Python adalah bahasa pemrograman tingkat tinggi yang terkenal karena sintaksnya relatif gampang dibaca. Python sering dipakai buat web, automation, data science, sampai machine learning.",
  "answer_format": "explanation",
  "context": [],
  "code": null,
  "code_generator": null,
  "language": "id",
  "register": "informal",
  "quality_score": 0.95
}

---

11. Schema Dataset Lengkap

Field wajib:

id
question
answer
category
topic
style
language

Field opsional:

context
answer_format
code
code_generator
difficulty
intent
entities
keywords
source
source_section
quality_score
safety_status

---

12. Question Categories

Generator wajib mendukung kategori berikut.

12.1 Definition

Contoh:

"Git itu apaan?"

"bro compiler tuh buat apa?"

"LLVM itu sebenarnya apa sih?"

12.2 Explanation

"jelasin compiler pake bahasa gampang dong"

"kenapa C++ lebih ribet dari Python?"

"kok program bisa jalan dari source code?"

12.3 How-to

"cara install Python gimana?"

"gimana cara compile C++?"

"cara bikin function di Nusa gimana?"

12.4 Troubleshooting

"bro error ini kenapa?"

"kok program gw crash?"

"build gagal terus dah"

"ini dependency error apaan?"

12.5 Comparison

"Python sama Java beda jauh gak?"

"var vs let bedanya apaan?"

"LLVM sama GCC mending mana?"

12.6 Recommendation

"buat pemula enaknya belajar bahasa apa?"

"kalau buat AI mending Python apa C++?"

"buat bikin compiler enaknya pake LLVM gak?"

12.7 Reasoning

"kenapa compiler butuh lexer?"

"kenapa model bisa overfit?"

"kenapa validation loss penting?"

12.8 Debugging

"ini code gw kenapa error?"

"tolong cek logic gw dong"

"kok outputnya aneh?"

12.9 Code Generation

"bikinin function Python dong"

"buat contoh loop di Nusa"

"bro generate parser sederhana"

12.10 Transformation

"ubah ini jadi bahasa gampang"

"jelasin versi anak baru belajar"

"bikin versi formal dong"

---

13. Question Style Generator

Setiap knowledge point harus dapat dibuat menjadi beberapa gaya.

Formal

Apa yang dimaksud dengan compiler?

Casual

Compiler itu sebenarnya apa?

Gen Z

Bro compiler tuh apaan sih?

Very Casual

compiler apaan dah?

Chat Style

bro gw masih bingung compiler itu ngapain

Typo

kompiler tu apaan?

compiler gunanya apaan sih

Singkatan

compiler itu buat apa btw?

Mixed

bro, compiler basically ngapain sih di programming?

---

14. Typo Generator

Generator dapat membuat typo realistis.

Contoh:

python -> pyhton
compiler -> compielr
function -> fucntion
variable -> varible
programming -> progrmming

Tetapi typo harus dibatasi.

Target:

5–10% sample

Jangan sampai mayoritas dataset typo.

---

15. Slang Variation Generator

Sistem membuat variasi:

tidak -> gak -> ga -> nggak
saya -> aku -> gw -> gue
kamu -> lu -> lo
mengapa -> kenapa -> kok
bagaimana -> gimana
seperti -> kayak
saja -> aja
untuk -> buat
dengan -> sama

Contoh:

Formal:
Bagaimana cara memperbaiki error tersebut?

Gen Z:
Gimana cara benerin error ini?

Very casual:
ini error benerinnya gimana dah?

---

16. Code-Switching

Dataset dapat mengandung campuran Bahasa Indonesia dan istilah Inggris yang lazim digunakan dalam teknologi.

Contoh:

"Kenapa build gw gagal?"

"How do I fix this error?"

"Ini function return value-nya kenapa null?"

"Compiler-nya nge-crash pas parsing."

Target:

5–10% dataset

---

17. Answer Style

Jawaban juga harus memiliki variasi.

Direct

Python adalah bahasa pemrograman tingkat tinggi.

Conversational

Python itu bahasa pemrograman yang cukup populer dan relatif gampang dipelajari.

Gen Z

Python itu bahasa pemrograman yang syntax-nya lumayan gampang dibaca, jadi enak banget buat mulai belajar coding.

Technical

Python merupakan bahasa pemrograman tingkat tinggi dengan dynamic typing dan garbage collection.

Step-by-step

1. Install Python.
2. Buat file .py.
3. Tulis program.
4. Jalankan dengan Python interpreter.

---

18. Aturan Answer

Jawaban wajib:

akurat
relevan
jelas
tidak bertele-tele
sesuai pertanyaan

Jawaban tidak boleh:

mengarang fakta
menjawab pertanyaan berbeda
menggunakan slang berlebihan
menghilangkan informasi penting

Prioritas:

Correctness > Relevance > Clarity > Style

---

19. Context Dataset

Dataset harus mendukung single-turn dan multi-turn.

Single-turn

{
  "question": "Python tuh apaan?",
  "answer": "Python adalah bahasa pemrograman tingkat tinggi..."
}

Multi-turn

{
  "context": [
    {
      "role": "user",
      "content": "Python tuh apaan?"
    },
    {
      "role": "assistant",
      "content": "Python adalah bahasa pemrograman..."
    }
  ],
  "question": "terus biasanya dipake buat apa?",
  "answer": "Python sering digunakan untuk..."
}

---

20. Multi-turn Conversation

Target distribusi:

Single-turn: 70%
2-turn:       15%
3-turn:       10%
4+ turn:       5%

Multi-turn harus mengandung referensi seperti:

itu
yang tadi
terus?
kenapa?
kalau gitu?
berarti gimana?

Contoh:

User:
LLVM itu apa?

Assistant:
LLVM adalah kumpulan compiler infrastructure...

User:
ohh terus gunanya buat bikin compiler sendiri?

Assistant:
Bisa. LLVM menyediakan komponen seperti IR...

User:
berarti gak harus bikin backend dari nol?

Assistant:
Betul. Kamu bisa menggunakan LLVM sebagai backend...

---

21. Context Resolution

Model harus belajar menyelesaikan referensi.

Contoh:

User:
Python itu interpreted language?

Assistant:
Python biasanya dijalankan menggunakan interpreter...

User:
terus yang bikin dia lambat itu itu ya?

Sistem harus menghasilkan answer yang memahami bahwa:

"itu"

mengacu pada topik sebelumnya.

---

22. Difficulty Level

Dataset dibagi:

easy
medium
hard
expert

Easy

Pertanyaan faktual.

Medium

Perlu penjelasan.

Hard

Memerlukan reasoning.

Expert

Memerlukan pemahaman teknis mendalam.

Target:

Easy      30%
Medium    35%
Hard      25%
Expert    10%

---

23. Reasoning Dataset

Dataset reasoning tidak hanya bertanya:

Apa itu compiler?

Tetapi:

Kenapa compiler gak langsung mengubah source code jadi machine code tanpa intermediate representation?

Jawaban harus menjelaskan hubungan konsep.

Contoh:

Compiler dapat menggunakan intermediate representation agar proses optimisasi dan target backend menjadi lebih modular. Satu frontend bisa menghasilkan IR yang kemudian diterjemahkan ke beberapa arsitektur...

---

24. Negative QA

Dataset harus memiliki contoh pertanyaan yang ambigu atau salah.

Contoh:

"Python itu dibuat Microsoft?"

Jawaban:

Bukan. Python awalnya dibuat oleh Guido van Rossum dan pertama kali dirilis pada awal 1990-an.

Tujuan:

mengurangi hallucination

---

25. Ambiguous Questions

Contoh:

"gimana caranya?"

"kenapa error?"

"itu buat apa?"

"bisa gak?"


Dataset harus menyediakan konteks yang membuat pertanyaan tersebut dapat dijawab.

---

26. Dataset Balance

Dataset final harus diseimbangkan berdasarkan:

topic
intent
difficulty
style
length
language register

Tidak boleh terjadi:

80% dataset = coding

kecuali memang tujuan model khusus coding.

---

27. Distribusi Topic

Contoh target:

Programming          25%
AI / ML              20%
Computer Science     15%
Software Engineering 15%
Linux / OS           10%
Web                  5%
Database              5%
General Knowledge     5%

Distribusi dapat dikonfigurasi.

---

28. Dataset Size

Pipeline harus mendukung:

1,000
10,000
50,000
100,000
500,000
1,000,000+

sample.

Parameter:

TARGET_SAMPLES = 100000

---

29. Data Augmentation

Satu knowledge point dapat menghasilkan:

formal question
casual question
Gen Z question
typo question
short question
long question
comparison question
why question
how question
follow-up question

Contoh:

Knowledge:

Git digunakan untuk version control.

Dapat menghasilkan:

Apa itu Git?

Git digunakan untuk apa?

Bro Git itu buat apa sih?

Kenapa developer pada pake Git?

Git sama GitHub itu sama gak?

Kalau gak pake Git emang kenapa?

Git penting gak buat pemula?

---

30. Quality Scoring

Setiap sample mendapatkan score:

factuality
relevance
grammar
naturalness
style
diversity
duplication

Contoh:

{
  "factuality": 0.98,
  "relevance": 0.96,
  "naturalness": 0.93,
  "diversity": 0.91,
  "final_score": 0.945
}

---

31. Quality Threshold

Default:

FINAL_SCORE >= 0.85

Sample:

< 0.85

masuk quarantine.

Sample:

< 0.70

dibuang.

---

32. Duplicate Detection

Dataset harus mendeteksi:

Exact duplicate

string sama

Near duplicate

Apa itu Python?

Python itu apa?

Semantic duplicate

Python digunakan untuk apa?

Apa kegunaan Python?

Tidak semuanya harus dibuang karena variasi intent dapat tetap bernilai.

---

33. Dataset Deduplication

Gunakan beberapa layer:

exact hash
normalized text hash
n-gram similarity
embedding similarity

Threshold semantic similarity configurable.

Contoh:

SIMILARITY_THRESHOLD = 0.92

---

34. Safety Filtering

Dataset harus melakukan filtering untuk:

violent instructions
self-harm instructions
illegal activity instructions
dangerous substance instructions
malware abuse
credential theft
privacy invasion

Tujuan utama:

menjaga dataset aman dan usable

---

35. PII Filtering

Dataset harus mendeteksi informasi sensitif seperti:

password
API key
token
private key
nomor kartu
alamat pribadi
nomor telepon
email pribadi

Data tersebut harus:

removed
masked

---

36. Code Dataset

Untuk QA coding:

{
  "question": "bro bikin function Python buat ngecek angka genap dong",
  "answer": "Bisa. Contohnya seperti ini:",
  "code": "def is_even(n):\n    return n % 2 == 0",
  "language": "python"
}

---

37. Code Validation

Setiap generated code sebaiknya melalui:

syntax validation
static analysis
unit test
execution sandbox

Contoh:

Python -> compile()
JavaScript -> parser
C/C++ -> compiler
Nusa -> Nusa compiler

---

38. Code Generator

Field:

"code_generator": {
  "language": "python",
  "purpose": "generate_even_checker",
  "generator": "generate_even_checker()"
}

Generator digunakan untuk sample yang membutuhkan code synthesis.

---

39. Metadata

Setiap sample menyimpan metadata:

{
  "id": "qa_00001",
  "source": "python_docs.md",
  "section": "Variables",
  "topic": "python",
  "category": "definition",
  "style": "gen_z",
  "difficulty": "easy",
  "generator_version": "1.0.0"
}

---

40. Dataset Split

Default:

Train      90%
Validation 5%
Test       5%

Alternative:

Train      80%
Validation 10%
Test       10%

Data split harus dilakukan setelah deduplication.

---

41. Leakage Prevention

Knowledge point yang sama atau hampir sama tidak boleh berada di:

train
validation
test

Contoh:

Train:

Python digunakan untuk data science.

Test tidak boleh:

Python biasanya dipakai buat data science.

jika secara semantik identik.

---

42. JSONL Format

Format utama:

{"id":"qa_000001","question":"Python tuh apaan?","answer":"Python adalah bahasa pemrograman tingkat tinggi.","category":"definition","style":"gen_z"}
{"id":"qa_000002","question":"Compiler itu buat apa sih?","answer":"Compiler menerjemahkan source code ke bentuk lain yang bisa diproses atau dijalankan oleh sistem.","category":"definition","style":"gen_z"}

---

43. ChatML Format

Untuk training chatbot:

{
  "messages": [
    {
      "role": "system",
      "content": "Kamu adalah assistant yang helpful dan informatif."
    },
    {
      "role": "user",
      "content": "bro compiler tuh apaan?"
    },
    {
      "role": "assistant",
      "content": "Compiler itu program yang nerjemahin source code ke bentuk yang bisa diproses komputer."
    }
  ]
}

---

44. Training Format

Pipeline harus mampu menghasilkan:

raw.jsonl
chatml.jsonl
instruction.jsonl
qa.jsonl
parquet

Dengan configuration:

output_format:
  - jsonl
  - chatml
  - parquet

---

45. Generator Configuration

Contoh:

dataset:
  target_samples: 100000

language:
  primary: id

style:
  formal: 15
  casual: 20
  gen_z: 30
  very_casual: 10
  mixed: 10
  semi_formal: 15

difficulty:
  easy: 30
  medium: 35
  hard: 25
  expert: 10

---

46. CLI

Pipeline menyediakan CLI:

python generate.py \
  --input docs/ \
  --output dataset/ \
  --samples 100000

Validation:

python validate.py dataset/train.jsonl

Deduplication:

python dedup.py dataset/

Statistics:

python stats.py dataset/

---

47. Dashboard Statistik

Generator menghasilkan laporan:

Total samples
Average question length
Average answer length
Category distribution
Style distribution
Difficulty distribution
Topic distribution
Duplicate rate
Rejected samples
Safety filtered samples

Contoh:

Total Samples       : 100,000
Accepted            : 96,421
Rejected            : 3,579
Duplicate           : 1,842
Average Q Length    : 8.7 tokens
Average A Length    : 46.2 tokens
Gen Z               : 30%

---

48. Naturalness Evaluation

Dataset harus dievaluasi apakah pertanyaannya terdengar natural.

Contoh buruk:

"Bro bestie literally apakah compiler itu merupakan perangkat lunak yang digunakan?"

Contoh natural:

"Bro compiler itu buat apa sih?"

Naturalness score:

0.0 - 1.0

---

49. Style Control

User dapat menjalankan generator:

--style gen_z

atau:

--style mixed

atau:

--style formal

Contoh:

python generate.py \
  --style gen_z \
  --samples 50000

---

50. Intensity Control

Gen Z tidak boleh binary.

Gunakan:

0.0 = formal
0.25 = semi-formal
0.50 = casual
0.75 = Gen Z
1.0 = very casual

Contoh:

gen_z_intensity: 0.75

---

51. Gen Z Vocabulary Engine

Vocabulary harus memiliki kategori:

addressing
reaction
filler
shortening
slang
internet slang
tech slang
mixed English

Contoh:

addressing:
bro
bang
guys
bestie

reaction:
wkwk
anjir
gila
buset

filler:
sih
dong
nih
deh
lah

shortening:
gak
ga
gw
lu
aja

Vocabulary harus dapat di-update tanpa mengubah core pipeline.

---

52. Anti-Over-Slang Rule

Sistem harus memiliki batas slang.

Misalnya:

maksimal 2–3 slang marker per question

Contoh valid:

"Bro, ini error kenapa sih?"

Contoh terlalu berlebihan:

"Bro anjir bestie literally error-nya vibes banget wkwk fr."

Sample seperti ini harus ditolak atau diregenerasi.

---

53. Grammar Control

Gen Z bukan berarti grammar selalu rusak.

Dataset harus mencakup:

grammar correct
informal grammar
chat grammar
intentional typo

Tujuannya agar model memahami berbagai register.

---

54. Length Distribution

Questions:

short      20%
medium     55%
long       25%

Answers:

one-liner     15%
short         35%
medium        35%
long          15%

---

55. Question Length Examples

Short:

"Git itu apa?"

Medium:

"Bro, Git itu sebenarnya dipakai buat apa?"

Long:

"Gw baru belajar coding dan sering dengar Git, tapi masih bingung sebenarnya Git itu buat nyimpen kode doang atau ada fungsi lain?"

---

56. Conversational Noise

Dataset dapat mencakup:

"eh"
"btw"
"bro"
"wait"
"sebentar"
"jadi..."

Tetapi noise tidak boleh mendominasi.

---

57. Intent Preservation

Transformation:

Formal:
Bagaimana cara melakukan debugging?

Gen Z:
Bro, cara debugging yang bener gimana?

Intent harus tetap:

debugging_method

Bukan berubah menjadi:

definition

---

58. Automated Validation

Validator memeriksa:

schema
empty fields
duplicate
fact consistency
length
language
style
safety
code syntax

Sample gagal validation:

status = rejected

---

59. Human Review

Dataset berkualitas tinggi tetap membutuhkan sampling manual.

Target:

100–500 samples per release

Reviewer memeriksa:

naturalness
factuality
style
answer quality

---

60. Golden Dataset

Project menyediakan:

golden_test.jsonl

Dataset kecil yang digunakan untuk:

regression testing
pipeline testing
model evaluation

---

61. Release Versioning

Format:

v0.1.0
v0.2.0
v0.3.0
...
v1.0.0

Metadata:

{
  "dataset_version": "1.0.0",
  "generator_version": "1.2.0",
  "samples": 100000,
  "language": "id",
  "style": "gen_z"
}

---

62. Reproducibility

Generator wajib menggunakan:

SEED = 42

Jika seed sama:

input sama
+
configuration sama
+
generator version sama

maka hasil harus sebisa mungkin reproducible.

---

63. Folder Structure

genz-qa/
│
├── raw/
├── processed/
├── generated/
├── validated/
├── rejected/
├── dataset/
│   ├── train.jsonl
│   ├── validation.jsonl
│   └── test.jsonl
│
├── metadata/
├── reports/
├── configs/
├── generators/
├── validators/
├── tests/
│
├── generate.py
├── validate.py
├── dedup.py
├── stats.py
└── README.md

---

64. Generator Modules

question_generator.py
answer_generator.py
genz_transformer.py
typo_generator.py
slang_engine.py
context_generator.py
code_generator.py
augmentation.py

---

65. Validator Modules

schema_validator.py
fact_validator.py
style_validator.py
duplicate_validator.py
safety_validator.py
code_validator.py

---

66. Pipeline API

Contoh Python API:

dataset = DatasetGenerator(
    input_path="docs/",
    target_samples=100000,
    style="gen_z",
    seed=42
)

dataset.generate()
dataset.validate()
dataset.deduplicate()
dataset.export()

---

67. Batch Generation

Generator harus dapat bekerja secara batch.

Contoh:

Batch 001
Batch 002
Batch 003
...
Batch 100

Tujuan:

hemat RAM
lebih mudah recovery
lebih mudah parallel processing

---

68. Resume Capability

Jika proses berhenti:

python generate.py --resume

Pipeline melanjutkan dari checkpoint terakhir.

---

69. Logging

Contoh:

[INFO] Reading documents...
[INFO] Found 1,243 sections
[INFO] Generating questions...
[INFO] Generated 100,000 samples
[INFO] Running validation...
[INFO] Accepted: 96,421
[INFO] Deduplicating...
[INFO] Final dataset: 95,214

---

70. Error Handling

Generator harus menangani:

empty document
invalid JSON
invalid Markdown
encoding error
generation failure
validation failure
code execution failure

Tanpa menghentikan seluruh pipeline kecuali error bersifat fatal.

---

71. Performance Requirements

Target:

10k samples -> < 5 min
100k samples -> scalable
1M samples -> batch processing

Actual performance tergantung hardware dan generator yang digunakan.

---

72. Acceptance Criteria

Produk dianggap berhasil apabila:

Dataset

- menghasilkan minimal 10.000 QA;
- schema valid 100%;
- duplicate rate rendah;
- train/val/test tidak leakage;
- memiliki variasi bahasa;
- memiliki variasi intent;
- memiliki variasi difficulty.

Gen Z Style

- pertanyaan terdengar natural;
- slang tidak berlebihan;
- typo realistis;
- chat style realistis;
- jawaban tetap informatif.

Technical

- pipeline dapat diulang;
- seed tersedia;
- checkpoint tersedia;
- CLI tersedia;
- dataset dapat diekspor ke JSONL.

---

73. Quality KPI

Target awal:

Schema validity       >= 99.9%
Factual accuracy      >= 95%
Naturalness           >= 90%
Duplicate rate        <= 3%
Train/test leakage    = 0%
Safety compliance     >= 99%

---

74. Example Final Samples

Sample 1

{
  "question": "Bro, compiler itu apaan sih?",
  "answer": "Compiler itu program yang menerjemahkan source code ke bentuk lain yang bisa diproses atau dijalankan oleh komputer. Tergantung compiler-nya, hasilnya bisa berupa machine code, object code, atau bentuk perantara.",
  "category": "definition",
  "style": "gen_z"
}

Sample 2

{
  "question": "gw baru belajar coding, Git tuh buat apaan?",
  "answer": "Git itu sistem version control. Simpelnya, Git bantu kamu nyatet perubahan kode, balik ke versi sebelumnya, dan kerja bareng orang lain tanpa berantakan.",
  "category": "definition",
  "style": "gen_z"
}

Sample 3

{
  "question": "ini Python kenapa error pas import module?",
  "answer": "Biasanya ada beberapa kemungkinan: module belum ter-install, nama module salah, atau environment Python yang dipakai berbeda dengan environment tempat module di-install. Coba cek dengan environment yang sedang menjalankan program.",
  "category": "troubleshooting",
  "style": "gen_z"
}

---

75. Example Multi-turn Dataset

{
  "messages": [
    {
      "role": "user",
      "content": "bro LLVM itu apaan?"
    },
    {
      "role": "assistant",
      "content": "LLVM itu kumpulan compiler infrastructure yang bisa dipakai buat bikin compiler, optimizer, dan toolchain."
    },
    {
      "role": "user",
      "content": "terus kalau gw bikin bahasa pemrograman sendiri kepake gak?"
    },
    {
      "role": "assistant",
      "content": "Kepake banget. LLVM bisa menangani bagian backend dan optimisasi, jadi kamu gak harus bikin semuanya dari nol."
    }
  ]
}

---

76. Recommended Dataset Composition

Untuk dataset awal 100.000 sample:

30,000 Gen Z
20,000 Casual
15,000 Semi-formal
15,000 Formal
10,000 Very casual
10,000 Mixed-language

Intent:

15,000 definition
15,000 explanation
15,000 how-to
15,000 troubleshooting
10,000 comparison
10,000 reasoning
10,000 debugging
5,000 code generation
5,000 transformation

---

77. Roadmap

Phase 0 — Foundation

dataset schema
input parser
JSONL exporter
configuration

Phase 1 — Basic Generator

question generation
answer generation
basic augmentation

Phase 2 — Gen Z Engine

slang
typo
casualization
code-switching
style intensity

Phase 3 — Quality System

validation
deduplication
factuality
quality scoring

Phase 4 — Conversation

multi-turn
context tracking
follow-up questions
reference resolution

Phase 5 — Code QA

code generation
debugging
syntax validation
execution testing

Phase 6 — Production

checkpoint
resume
batch processing
parallelization
reporting
versioning

Phase 7 — Dataset v1.0

Target:

100k–1M samples
high diversity
high factuality
zero test leakage
production-ready format

---

78. Definition of Done

Project dinyatakan DONE ketika pipeline mampu melakukan:

README / Markdown
      ↓
Parse
      ↓
Knowledge Extraction
      ↓
QA Generation
      ↓
Gen Z Transformation
      ↓
Conversation Generation
      ↓
Code Generation
      ↓
Validation
      ↓
Deduplication
      ↓
Safety Filtering
      ↓
Balancing
      ↓
Train/Val/Test Split
      ↓
JSONL Export

dan menghasilkan dataset yang:

Natural
+
Diverse
+
Accurate
+
Conversational
+
Gen Z
+
Technically Useful
+
Training Ready

---

79. Prinsip Akhir

Konsep utama produk:

«"Ngomongnya boleh santai, datanya jangan ngawur."»

Dataset harus membuat model memahami cara manusia Gen Z bertanya tanpa mengorbankan factuality, reasoning, dan kualitas jawaban.

Target akhirnya bukan sekadar model yang bisa berkata:

"wkwk bro"

tetapi model yang bisa memahami:

"bro gw udah coba build tapi compiler gw ngambek terus, error-nya bilang undefined symbol. ini apaan dah?"

dan memberikan jawaban teknis yang benar, jelas, serta tetap terasa natural dalam konteks percakapan.