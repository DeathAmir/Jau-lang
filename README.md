<div align="center">

<img src="https://capsule-render.vercel.app/api?type=waving&height=300&color=0f172a&text=Jau%20Language&fontColor=ffffff&fontSize=60&animation=fadeIn" width="100%"/>

<!-- New Jau Logo -->
<svg width="120" height="120" viewBox="0 0 120 120">
  <defs>
    <linearGradient id="jauGradient" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" style="stop-color:#4F46E5;stop-opacity:1" />
      <stop offset="100%" style="stop-color:#7C3AED;stop-opacity:1" />
    </linearGradient>
  </defs>
  <path d="M60 20 L90 60 L60 100 L30 60 Z" fill="url(#jauGradient)" stroke="#0f172a" stroke-width="3"/>
  <text x="60" y="65" text-anchor="middle" fill="white" font-size="24" font-weight="bold">J</text>
</svg>

<h1>Jau Programming Language</h1>
<h3>You break it. <b>Jau</b> fixes it.</h3>

<br>

<div>
  <img src="https://svg.shields.io/badge/version-0.1.0-blue?logo=semver&logoColor=white">
  <img src="https://svg.shields.io/badge/runtime-JUR-orange">
  <img src="https://svg.shields.io/badge/platform-cross--platform-green">
  <img src="https://svg.shields.io/badge/status-experimental-red">
</div>

<br>

<div>
  <img src="https://svg.shields.io/github/stars/DeathAmir/Jau?color=yellow">
  <img src="https://svg.shields.io/github/forks/DeathAmir/Jau?color=orange">
  <img src="https://svg.shields.io/github/issues/DeathAmir/Jau?color=red">
  <img src="https://svg.shields.io/github/license/DeathAmir/Jau?color=purple">
</div>

<br><br>

<svg width="400" height="60" viewBox="0 0 400 60">
  <image href="https://skillicons.dev/icons?i=linux" x="0" height="45"/>
  <image href="https://skillicons.dev/icons?i=windows" x="60" height="45"/>
  <image href="https://skillicons.dev/icons?i=macos" x="120" height="45"/>
  <image href="https://skillicons.dev/icons?i=android" x="180" height="45"/>
  <image href="https://skillicons.dev/icons?i=ios" x="240" height="45"/>
  <image href="https://skillicons.dev/icons?i=wasm" x="300" height="45"/>
</svg>

</div>

---

<div align="center">

<h1>⚙️ Jau Identity</h1>

<svg width="200" height="100" viewBox="0 0 200 100">
  <path d="M40 20 L80 20 L120 60 L80 100 L40 100 L0 60 Z" fill="#0f172a" stroke="#7C3AED" stroke-width="2"/>
  <text x="60" y="60" text-anchor="middle" fill="white" font-size="24" font-weight="bold">J</text>
</svg>

</div>

---

# English

## What is Jau
Jau is a modern experimental programming language focused on **speed**, **simplicity**, and **hardware‑level performance** without the painful complexity of traditional low‑level languages.

---

## Key Features
<div>
  <svg width="300" height="300" viewBox="0 0 300 300">
    <circle cx="150" cy="150" r="140" fill="#0f172a" stroke="#7C3AED" stroke-width="2"/>
    <text x="150" y="100" text-anchor="middle" fill="white" font-size="20">🚀 Ultra Fast Compilation</text>
    <text x="150" y="140" text-anchor="middle" fill="white" font-size="20">🧠 Simple Clean Syntax</text>
    <text x="150" y="180" text-anchor="middle" fill="white" font-size="20">🔒 Safe Runtime (JUR)</text>
    <text x="150" y="220" text-anchor="middle" fill="white" font-size="20">📦 Modular Package System</text>
  </svg>
</div>

---

## Architecture
```mermaid
graph LR
A[Jau Source .jau] --> B[Jau Compiler]
B --> C[Jau Bytecode .jbc]
C --> D[JUR Runtime]
D --> E[Native Execution]

---

## Example Code
rust
func quantumSort(arr) {
  if arr.length <= 1 return arr
  pivot = arr[0]
  left = quantumFilter(arr[1:], x => x < pivot)
  right = quantumFilter(arr[1:], x => x >= pivot)
  return quantumSort(left) + [pivot] + quantumSort(right)
}

---

## Toolchain
<div>
  <svg width="500" height="100" viewBox="0 0 500 100">
<rect x="0" y="0" width="120" height="40" fill="#4F46E5" rx="5"/>
<text x="60" y="25" text-anchor="middle" fill="white">jauc</text>

<rect x="130" y="0" width="120" height="40" fill="#7C3AED" rx="5"/>
<text x="190" y="25" text-anchor="middle" fill="white">jur</text>

<rect x="260" y="0" width="120" height="40" fill="#4F46E5" rx="5"/>
<text x="320" y="25" text-anchor="middle" fill="white">jaupm</text>

<rect x="390" y="0" width="120" height="40" fill="#7C3AED" rx="5"/>
<text x="450" y="25" text-anchor="middle" fill="white">jaufmt</text>
  </svg>
</div>

---

## Performance Vision
mermaid
pie
  title Performance Comparison
  "Jau" : 45
  "C++" : 30
  "Rust" : 20
  "Go" : 5

---

## Installation
bash
curl -sSf https://jau-lang.org/install.sh | sh

---

## Roadmap
<svg width="600" height="200" viewBox="0 0 600 200">
  <line x1="50" y1="100" x2="550" y2="100" stroke="#4F46E5" stroke-width="3"/>
  
  <circle cx="50" cy="100" r="10" fill="#7C3AED"/>
  <text x="50" y="140" text-anchor="middle">Compiler</text>
  
  <circle cx="150" cy="100" r="10" fill="#7C3AED"/>
  <text x="150" y="140" text-anchor="middle">Runtime</text>
  
  <circle cx="250" cy="100" r="10" fill="#4F46E5"/>
  <text x="250" y="140" text-anchor="middle">Wasm</text>
  
  <circle cx="350" cy="100" r="10" fill="#4F46E5"/>
  <text x="350" y="140" text-anchor="middle">Cloud</text>
  
  <circle cx="450" cy="100" r="10" fill="#4F46E5"/>
  <text x="450" y="140" text-anchor="middle">Debugger</text>
  
  <circle cx="550" cy="100" r="10" fill="#4F46E5"/>
  <text x="550" y="140" text-anchor="middle">LSP</text>
</svg>

---

## Ecosystem
<div>
  <svg width="400" height="200" viewBox="0 0 400 200">
<circle cx="200" cy="100" r="80" fill="#0f172a" stroke="#7C3AED" stroke-width="2"/>
<text x="200" y="70" text-anchor="middle" fill="white">JUR Runtime</text>
<text x="200" y="100" text-anchor="middle" fill="white">JauPM</text>
<text x="200" y="130" text-anchor="middle" fill="white">Standard Library</text>
  </svg>
</div>

---

<div align="center">

<svg width="300" height="80" viewBox="0 0 300 80">
  <rect width="300" height="80" fill="#0f172a" rx="10"/>
  <text x="150" y="40" text-anchor="middle" fill="white" font-size="20">Copyright © DeathAmir 2026</text>
</svg>

<br><br>

<img src="https://capsule-render.vercel.app/api?type=waving&height=120&color=0f172a&section=footer"/>

</div>
