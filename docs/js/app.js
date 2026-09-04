(function () {
  const items = [
    { title: "RECON", sub: "Scan / map targets", icon: "assets/icon_recon.png" },
    { title: "DEAUTH", sub: "Kick clients off AP", icon: "assets/icon_deauth.png" },
    { title: "EVIL TWIN", sub: "Rogue AP tools", icon: "assets/icon_evil.png" },
    { title: "BT ATTACKS", sub: "BLE HID tools", icon: "assets/icon_bt.png" },
    { title: "REMOTE", sub: "Remote ops", icon: "assets/icon_remote.png" },
    { title: "EVIL BT", sub: "Scan + clone BLE ads", icon: "assets/icon_bt.png" },
    { title: "RESOURCES", sub: "Heap flash CPU load", icon: "assets/icon_recon.png" },
  ];

  const boot = document.getElementById("boot");
  const splash = document.getElementById("boot-splash");
  window.setTimeout(function () {
    splash.classList.add("is-glitch");
    window.setTimeout(function () {
      boot.classList.add("is-gone");
      window.setTimeout(function () {
        boot.remove();
      }, 420);
    }, 1100);
  }, 900);

  let i = 2;
  const title = document.getElementById("menu-title");
  const sub = document.getElementById("menu-sub");
  const icon = document.getElementById("menu-icon");

  function paint() {
    const item = items[i];
    title.textContent = item.title;
    sub.textContent = item.sub;
    icon.src = item.icon;
    icon.alt = item.title;
  }

  document.getElementById("menu-prev").addEventListener("click", function () {
    i = (i + items.length - 1) % items.length;
    paint();
  });
  document.getElementById("menu-next").addEventListener("click", function () {
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

  const box = document.getElementById("lightbox");
  const boxImg = document.getElementById("lightbox-img");
  document.querySelectorAll(".shot[data-full]").forEach(function (btn) {
    btn.addEventListener("click", function () {
      boxImg.src = btn.getAttribute("data-full");
      boxImg.alt = btn.querySelector("img").alt;
      box.showModal();
    });
  });
})();
