(function () {
  document.documentElement.classList.add("js");

  const items = [
    { title: "RECON", sub: "Scan / map targets", icon: "assets/icon_recon.png" },
    { title: "DEAUTH", sub: "Kick clients off AP", icon: "assets/icon_deauth.png" },
    { title: "EVIL TWIN", sub: "Rogue AP tools", icon: "assets/icon_evil.png" },
    { title: "BT ATTACKS", sub: "BLE HID tools", icon: "assets/icon_bt.png" },
    { title: "REMOTE", sub: "Remote ops", icon: "assets/icon_remote.png" },
    { title: "EVIL BT", sub: "Scan + clone BLE ads", icon: "assets/icon_bt.png" },
    { title: "RESOURCES", sub: "Heap flash CPU load", icon: "assets/icon_recon.png" },
  ];

  function goLive() {
    document.body.classList.add("is-live");
  }

  function initReveal() {
    const els = document.querySelectorAll(".section, .warn, .foot");
    if (!("IntersectionObserver" in window)) {
      els.forEach(function (el) {
        el.classList.add("is-in");
      });
      return;
    }

    const io = new IntersectionObserver(
      function (entries) {
        entries.forEach(function (entry) {
          if (!entry.isIntersecting) return;
          entry.target.classList.add("is-in");
          io.unobserve(entry.target);
        });
      },
      { threshold: 0.12, rootMargin: "0px 0px -32px 0px" }
    );

    els.forEach(function (el) {
      el.classList.add("reveal");
      io.observe(el);
    });
  }

  const boot = document.getElementById("boot");
  const splash = document.getElementById("boot-splash");
  if (boot && splash) {
    window.setTimeout(function () {
      splash.classList.add("is-glitch");
      window.setTimeout(function () {
        boot.classList.add("is-gone");
        goLive();
        window.setTimeout(function () {
          boot.remove();
        }, 420);
      }, 1100);
    }, 900);
  } else {
    goLive();
  }

  initReveal();

  const title = document.getElementById("menu-title");
  const sub = document.getElementById("menu-sub");
  const icon = document.getElementById("menu-icon");
  const prev = document.getElementById("menu-prev");
  const next = document.getElementById("menu-next");
  const focus = document.querySelector(".menu__focus");

  if (title && sub && icon && prev && next) {
    let i = 2;

    function paint() {
      const item = items[i];
      title.textContent = item.title;
      sub.textContent = item.sub;
      icon.src = item.icon;
      icon.alt = item.title;
      if (focus) {
        focus.classList.remove("is-glitch");
        void focus.offsetWidth;
        focus.classList.add("is-glitch");
      }
    }

    prev.addEventListener("click", function () {
      i = (i + items.length - 1) % items.length;
      paint();
    });
    next.addEventListener("click", function () {
      i = (i + 1) % items.length;
      paint();
    });

    window.addEventListener("keydown", function (ev) {
      if (ev.key === "ArrowLeft") {
        i = (i + items.length - 1) % items.length;
        paint();
      } else if (ev.key === "ArrowRight") {
        i = (i + 1) % items.length;
        paint();
      }
    });
  }

  const box = document.getElementById("lightbox");
  const boxImg = document.getElementById("lightbox-img");
  if (box && boxImg) {
    document.querySelectorAll(".shot[data-full]").forEach(function (btn) {
      btn.addEventListener("click", function () {
        boxImg.src = btn.getAttribute("data-full");
        boxImg.alt = btn.querySelector("img").alt;
        box.showModal();
      });
    });
  }
})();
