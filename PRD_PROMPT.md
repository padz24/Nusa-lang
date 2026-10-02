Anda adalah seorang Product Manager, Software Architect, dan Technical Writer senior.

Buatkan PRD (Product Requirements Document) lengkap untuk membangun website dokumentasi resmi bahasa pemrograman Nusa.

Konteks Produk:

Nusa adalah bahasa pemrograman compiled modern dengan ekstensi file `.ns`, menggunakan backend LLVM 21.1.8, dikembangkan untuk Android AArch64/Termux.

Nusa memiliki fitur:
- Compiler sendiri
- LLVM backend
- Native executable generation
- Static typing
- Functions
- Struct
- Generics
- Traits
- Enum
- Pattern matching
- Option/Result
- Closures
- Concurrency (thread/channel/atomic)
- Package manager
- Registry
- C FFI
- Cross compilation
- LSP
- Debug information
- Bootstrap compiler
- Language specification resmi

Website dokumentasi harus menjadi pusat informasi resmi untuk:
- Pengguna baru yang ingin belajar Nusa
- Developer yang ingin menggunakan bahasa Nusa
- Developer compiler yang ingin memahami internal compiler
- Kontributor open-source
- AI assistant seperti Nusa Helper

Tujuan utama:

Membuat website dokumentasi seperti:
- Rust Documentation
- Go Documentation
- Swift Documentation
- LLVM Documentation

Tetapi memiliki identitas sendiri:
- Modern
- Developer-focused
- Offline-first
- Cepat
- Mudah dipelajari
- Mudah digunakan sebagai sumber dataset AI


Buat PRD dengan struktur berikut:


# 1. Product Overview

Jelaskan:
- Nama produk
- Visi produk
- Misi produk
- Masalah yang ingin diselesaikan
- Nilai utama website


# 2. Target User

Buat persona lengkap:

## Beginner Developer
- kebutuhan
- pain point
- solusi website

## Advanced Developer
- kebutuhan
- pain point
- solusi website

## Compiler Developer
- kebutuhan
- pain point
- solusi website

## Contributor
- kebutuhan
- pain point
- solusi website


# 3. Product Goals

Buat:
- Primary goals
- Secondary goals
- Non-goals


# 4. Information Architecture

Buat struktur folder dan navigasi website:

Contoh:

website/

├── learn/
├── language/
├── reference/
├── stdlib/
├── compiler/
├── ecosystem/
├── tutorials/
├── examples/
├── rfcs/
├── changelog/
├── downloads/


Jelaskan fungsi setiap bagian.


# 5. Core Features

Jelaskan fitur website:


## Homepage

Harus berisi:
- Hero section
- Introduction Nusa
- Feature highlights
- Installation button
- Quick start


## Learning System

Buat sistem pembelajaran:

- Beginner path
- Intermediate path
- Advanced path

Setiap tutorial memiliki:

- Penjelasan
- Source code `.ns`
- Output program
- Exercise
- Next step


## Language Reference

Buat dokumentasi:

- Syntax
- Variables
- Functions
- Types
- Struct
- Enum
- Generics
- Traits
- Pattern Matching
- Error Handling
- Concurrency
- Modules


Format setiap halaman:

- Overview
- Syntax
- Example
- Rules
- Compiler behavior
- Limitations
- Related topics


## Standard Library Documentation

Buat dokumentasi API:

- Module
- Function
- Parameter
- Return value
- Example
- Error behavior


## Compiler Documentation

Buat dokumentasi internal:

- Compiler pipeline

Source:

Lexer

↓

Parser

↓

AST

↓

Semantic Analysis

↓

Type Checker

↓

LLVM Backend

↓

Native Binary


Dokumentasikan:

- Architecture
- Compiler design
- Runtime
- Memory model
- LLVM integration


## RFC Documentation

Buat sistem RFC:

- Proposed
- Accepted
- Implemented
- Rejected

Dengan template:

- Motivation
- Design
- Alternatives
- Implementation
- Testing


## Changelog System

Website harus otomatis membaca CHANGELOG.md.

Buat:

- Version page
- Release notes
- Feature timeline


# 6. Search System

Buat spesifikasi search:

Support:

- Keyword search
- Code search
- API search
- Error message search
- Version filtering


Contoh:

Input:

"chan_len"

Output:

- Concurrency documentation
- Standard library
- Changelog


# 7. Code Documentation System

Buat fitur:

- Syntax highlighting
- Copy code button
- Code playground
- Output preview


Support:

- Nusa
- C++
- LLVM IR
- Bash
- JSON
- TOML


# 8. Interactive Playground

Buat desain:

Browser

↓

Playground API

↓

Compiler Service

↓

Sandbox

↓

Output


Jelaskan:

- Security model
- Resource limitation
- Error handling


# 9. AI Documentation Support

Website harus siap digunakan untuk training AI.

Buat:

Dataset export format:

{
"title":"",
"category":"",
"version":"",
"content":"",
"code_example":"",
"keywords":""
}


Jelaskan bagaimana dokumentasi dapat digunakan oleh:

- Retrieval system
- Chat QA
- Nusa Helper


# 10. UI/UX Design

Buat:

Design system:

Tema:

"Nusa Terminal Theme"


Karakter:

- Dark mode
- Developer style
- Modern
- Minimal


Tentukan:

- Color system
- Typography
- Layout
- Navigation
- Responsive behavior


# 11. Technical Specification

Buat rekomendasi:

Frontend:
- Static HTML/CSS/JS atau framework modern

Documentation engine:
- Markdown parser
- Static generator

Deployment:
- GitHub Pages
- Cloudflare Pages
- Local hosting


Requirement:

- Fast loading
- Offline support
- SEO friendly
- Accessibility


# 12. Version Roadmap

Buat roadmap:

## Nusa Docs v0.1.0

Basic documentation

## v0.2.0

Reference system

## v0.3.0

Compiler documentation

## v0.4.0

Interactive playground

## v0.5.0

AI documentation layer


# 13. Database / File Structure

Jika menggunakan static system:

Buat struktur:

docs/

website/

scripts/

assets/


Jika menggunakan backend:

Buat:

- Database schema
- API endpoint


# 14. Security Requirement

Jelaskan:

- Sandbox security
- XSS protection
- Content validation
- Safe code execution


# 15. Performance Requirement

Target:

- Lighthouse score >95
- First load <1 second
- Mobile friendly
- Minimal JavaScript


# 16. Definition of Done

Buat checklist:

- Dokumentasi lengkap
- Search bekerja
- Tutorial tersedia
- API reference tersedia
- Compiler docs tersedia
- Mobile responsive
- Offline support
- AI dataset export tersedia


# Output Format

Gunakan format PRD profesional:

- Markdown
- Heading jelas
- Table jika diperlukan
- Requirement ID seperti:

REQ-001
REQ-002
REQ-003

Tambahkan:

- User story
- Acceptance criteria
- Technical notes

Jangan membuat dokumentasi isi Nusa, tetapi buat PRD untuk membangun website dokumentasinya.
