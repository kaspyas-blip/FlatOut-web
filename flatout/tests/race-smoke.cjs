const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch({ headless: false, args: ['--enable-unsafe-webgpu', '--enable-features=Vulkan', '--use-vulkan=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--autoplay-policy=no-user-gesture-required'] });
  const p = await b.newPage({ viewport: { width: 1280, height: 800 }, locale: 'cs-CZ' });
  p.on('console', m => console.log(m.text()));
  p.on('pageerror', e => console.log('PAGEERROR', e.message));
  await p.goto('http://localhost:8080/');
  await p.screenshot({ path: './b0.png', fullPage: true });
  await p.waitForSelector('#play:not([disabled])', { timeout: 90000 });
  await p.click('#play');
  const steps = [[150,'Enter'],[200,'Enter'],[240,'ArrowDown'],[250,'Enter'],[290,'Enter'],[320,'Enter'],[350,'Enter'],[380,'Enter'],[410,'Enter'],[440,'Enter'],[470,'Enter'],[500,'Enter'],[530,'Enter'],[560,'Enter'],[590,'Enter'],[640,'down:ArrowUp'],[700,'down:ArrowLeft'],[703,'up:ArrowLeft'],[760,'down:ArrowRight'],[763,'up:ArrowRight']];
  const t0 = Date.now(); let k = 0, i = 0;
  while (Date.now() - t0 < 780000) {
    await new Promise(r => setTimeout(r, 5000));
    const s = (Date.now() - t0) / 1000;
    while (k < steps.length && s >= steps[k][0]) { { const a = steps[k][1]; if (a.startsWith('down:')) await p.keyboard.down(a.slice(5)); else if (a.startsWith('up:')) await p.keyboard.up(a.slice(3)); else await p.keyboard.press(a); } console.log('KEY', steps[k][1], s); k++; }
    if (++i % 4 === 0) await p.screenshot({ path: `./y${i / 4}.png` }).catch(e => console.log('SHOT', e.message));
  }
  await b.close();
})();
