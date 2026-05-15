<div align="center">

<img src="https://capsule-render.vercel.app/api?type=waving&height=320&color=0f172a&text=Jau%20Language&fontColor=ffffff&fontSize=70&animation=fadeIn&fontAlignY=35" width="100%"/>

<h1><img src="https://raw.githubusercontent.com/Tarikul-Islam-Anik/Animated-Fluent-Emojis/master/Emojis/Objects/Zap.png" width="40"> Jau Programming Language</h1>

<h3>"You break it. <b>Jau</b> fixes it."</h3>

<p align="center">
  <img src="https://img.shields.io/github/stars/DeathAmir/Jau?style=for-the-badge&logo=github&color=FFD700&labelColor=1e293b">
  <img src="https://img.shields.io/github/forks/DeathAmir/Jau?style=for-the-badge&logo=git&color=orange&labelColor=1e293b">
  <img src="https://img.shields.io/github/issues/DeathAmir/Jau?style=for-the-badge&logo=github-actions&color=ef4444&labelColor=1e293b">
  <img src="https://img.shields.io/github/license/DeathAmir/Jau?style=for-the-badge&logo=balance-scale&color=a855f7&labelColor=1e293b">
  <img src="https://img.shields.io/github/repo-size/DeathAmir/Jau?style=for-the-badge&logo=database&color=10b981&labelColor=1e293b">
</p>

<br>

<p align="center">
  <img src="https://img.shields.io/badge/version-0.1.0-3b82f6?style=flat-square&logo=semver">
  <img src="https://img.shields.io/badge/runtime-JUR-f59e0b?style=flat-square&logo=engine">
  <img src="https://img.shields.io/badge/platform-cross--platform-10b981?style=flat-square&logo=target">
  <img src="https://img.shields.io/badge/status-experimental-ef4444?style=flat-square&logo=test-run">
</p>

<br>

<a href="#english">🇬🇧 English</a> • <a href="#persian">🇮🇷 فارسی</a>

<br><br>

<img src="https://skillicons.dev/icons?i=cpp,go,python,rust,linux,vscode,git,docker,wasm,githubactions,cmake" height="50">

</div>

---

<div align="center">

# 🛠️ System Architecture

```mermaid
graph LR
    subgraph Frontend
    A[".jau Source"] --> B["Lexer/Parser"]
    B --> C["AST Generation"]
    end
    
    subgraph Optimizer
    C --> D["Semantic Analysis"]
    D --> E["J-Optimizer"]
    end
    
    subgraph Backend
    E --> F["JBC Bytecode (.jbc)"]
    F --> G["JUR VM Runtime"]
    G --> H["Hardware Abstraction"]
    end
    
    style G fill:#f96,stroke:#333,stroke-width:4px
```

</div>

---

# English

## 🚀 The Vision

Jau is not just another language; it's a high-performance **Experimental Ecosystem**. It bridges the gap between Python's developer experience and C++'s raw power. No garbage collector overhead, just pure logic and the **JUR (Jau Universal Runtime)**.

---

## 💎 Elite Features

- **⚡ Blazing Compilation:** Powered by a parallelized multi-stage compiler.
- **🛡️ Memory Safety:** Ownership-inspired memory management without the "Borrow Checker" headache.
- **🧬 Native Interop:** Call C/C++ functions directly without boilerplate.
- **📦 JauPM:** A decentralized, lightning-fast package manager.
- **🌐 WASM Ready:** Compile your logic to the web with one flag.
- **🧩 Hot Reloading:** Modify JUR bytecode on the fly.

---

## 🧪 Code Showcase

### Advanced Types & Control Flow
```rust
^ Types & Pattern ^

const VERSION = 1.0
mut status = "active"

func check(val) {
    match val {
        "Jau" => print("Native Speed Detected")
        _     => print("General Context")
    }
}

check("Jau")
```

### High-Level Performance
```rust
^ Concurrent Logic ^

spawn func heavy_task(id) {
    print("Task " + id + " running on JUR core")
}

for i in 0..10 {
    heavy_task(i)
}
```

---

## 📊 Benchmarks (Projected)

| Metric | Python | Go | C++ | **Jau** |
| :--- | :---: | :---: | :---: | :---: |
| **Startup Time** | 🐢 Slow | 🚀 Fast | 🔥 Instant | **🚀 Fast** |
| **Memory Footprint** | 🐘 Large | 🐕 Medium | 🐜 Small | **🐜 Small** |
| **Development Speed** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐ | **⭐⭐⭐⭐⭐** |
| **Safety** | 🛡️ | 🛡️🛡️ | 💀 | **🛡️🛡️🛡️** |

---

## 📅 Future Roadmap

- [x] **Phase 1:** Core Grammar & Compiler Base
- [x] **Phase 2:** JUR (Jau Universal Runtime) Implementation
- [ ] **Phase 3:** JauPM Cloud Registry & Dependency Graph
- [ ] **Phase 4:** LLVM Backend Integration
- [ ] **Phase 5:** Jau-UI Framework for Native Desktop Apps
- [ ] **Phase 6:** Standard Library (I/O, Crypto, Net)

---

# Persian

<div dir="rtl">

## 🚀 چشم‌انداز Jau

جاو فقط یک زبان نیست؛ یک **اکوسیستم آزمایشی** با کارایی بالاست. هدف ما پر کردن شکاف بین سادگی پایتون و قدرت بی حد و مرز C++ است. بدون سنگینی Garbage Collector، فقط منطق خالص و **رانتایم JUR**.

---

## 💎 ویژگی‌های کلیدی

- **⚡ کامپایل فوق‌سریع:** بهره‌گیری از معماری موازی در پردازش کدها.
- **🛡️ امنیت حافظه:** مدیریت هوشمند حافظه بدون درگیری با مفاهیم پیچیده.
- **🧬 ارتباط بومی:** فراخوانی مستقیم توابع C/C++ بدون اتلاف وقت.
- **📦 مدیریت پکیج:** سیستم JauPM برای مدیریت وابستگی‌ها با سرعت نور.
- **🌐 آماده برای وب:** قابلیت خروجی گرفتن مستقیم برای WebAssembly.

---

## 🛠 ابزارهای توسعه

| ابزار | نقش در سیستم | وضعیت |
|:---:|:---:|:---:|
| **jauc** | کامپایلر اصلی به بایت‌کد | ✅ پایدار |
| **jur** | رانتایم اجرای بایت‌کد | ✅ پایدار |
| **jaupm** | مدیریت بسته‌ها و کتابخانه‌ها | ⏳ در حال توسعه |
| **jaufmt** | مرتب‌ساز هوشمند کد | ⏳ در حال توسعه |
| **jauls** | پروتکل LSP برای VSCode | 📅 برنامه‌ریزی شده |

---

## مشارکت در پروژه

ما به دنبال توسعه‌دهندگانی هستیم که می‌خواهند آینده برنامه‌نویسی را تغییر دهند. اگر در زمینه کامپایلرها، رانتایم‌ها یا بهینه‌سازی کد تجربه دارید، Jau منتظر شماست.

</div>

---

<div align="center">

## 🌟 Support The Revolution

If you like what we're building, give us a star! It keeps the JUR engines running.

<img src="https://img.shields.io/github/stars/DeathAmir/Jau?style=social">

<br>

`Built with ❤️ by DeathAmir`

<img src="https://capsule-render.vercel.app/api?type=waving&height=120&color=0f172a&section=footer"/>

</div>
