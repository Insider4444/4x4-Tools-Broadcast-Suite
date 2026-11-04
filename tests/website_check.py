"""Local browser checks for the public one-download flow (no real downloads)."""
from pathlib import Path
import json
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.tools/browser-checks'))
from playwright.sync_api import sync_playwright


def main():
    url=sys.argv[1] if len(sys.argv)>1 else 'http://127.0.0.1:4173'
    (ROOT/'build').mkdir(exist_ok=True)
    with sync_playwright() as p:
        browser=p.chromium.launch(channel='chrome',headless=True)
        context=browser.new_context(viewport={'width':1440,'height':1100},reduced_motion='reduce')
        page=context.new_page();errors=[];requests=[]
        page.on('pageerror',lambda e:errors.append(str(e)))
        page.on('request',lambda r:requests.append(r.url))
        page.goto(url,wait_until='networkidle')
        for width in [1440,1024,768,390,320]:
            page.set_viewport_size({'width':width,'height':1100 if width>760 else 950})
            metrics=page.evaluate('''() => ({viewport:innerWidth,width:document.documentElement.scrollWidth,overflow:[...document.querySelectorAll('body *')].map(e=>({tag:e.tagName,c:e.className,r:e.getBoundingClientRect().right,w:e.getBoundingClientRect().width})).filter(e=>e.r>innerWidth+1).slice(0,12)})''')
            page.screenshot(path=str(ROOT/'build'/f'website-{width}.png'),full_page=False)
            if metrics['width']>metrics['viewport']:
                raise RuntimeError('Horizontal overflow: '+json.dumps(metrics))
        page.set_viewport_size({'width':1440,'height':1100})
        page.get_by_role('tab',name='03 Let your voice lead').click()
        assert page.locator('#effect-pills').locator('span').count()==6
        page.get_by_role('tab',name='03 Let your voice lead').press('ArrowLeft')
        assert page.locator('#tab-background').get_attribute('aria-selected')=='true'
        page.locator('#tab-background').press('Home')
        assert page.locator('#tab-presence').get_attribute('aria-selected')=='true'
        page.locator('#tab-presence').press('End')
        assert page.locator('#tab-detail').get_attribute('aria-selected')=='true'
        page.locator('.full-toolkit summary').click()
        assert page.locator('.all-effects li').count()==13
        page.locator('.questions summary').first.click()
        assert page.locator('.questions details').first.get_attribute('open') is not None
        links=page.locator('a.download').evaluate_all('(elements)=>elements.map(e=>e.href)')
        expected='https://github.com/Insider4444/4x4-Tools-Broadcast-Suite/releases/latest/download/4x4Tools-Setup.exe'
        assert len(links)==3 and all(u==expected for u in links)
        page.evaluate("document.addEventListener('click',e=>{if(e.target.closest('a.download'))e.preventDefault()},true)")
        page.locator('a.download').first.click()
        assert page.locator('#download-status').is_visible()
        page.get_by_role('button',name='Dismiss download message').click()
        assert page.locator('#download-status').is_hidden()
        assert not errors,errors
        assert all(u.startswith(url.rstrip('/')) for u in requests),requests
        assert page.locator('[data-demo]').count()==4
        assert page.locator('video[src]').count()==0, 'Reduced motion should not automatically load external media'
        if '--media' in sys.argv:
            for figure in page.locator('[data-demo]').all():
                figure.scroll_into_view_if_needed()
                figure.locator('.motion-toggle').click()
                figure.locator('video').evaluate('(video)=>new Promise((resolve,reject)=>{const deadline=setTimeout(()=>reject(new Error("Demo failed to play: "+video.dataset.src)),30000);const poll=setInterval(()=>{if(video.currentTime>0&&!video.paused){clearTimeout(deadline);clearInterval(poll);resolve()}},100)})')
                assert 'loaded' in figure.get_attribute('class')
                figure.locator('.motion-toggle').click()
                assert figure.locator('video').evaluate('(video)=>video.paused')
            allowed=('https://www.nvidia.com/','https://images.nvidia.com/')
            assert all(u.startswith(url.rstrip('/')) or u.startswith(allowed) for u in requests),requests
            page.set_viewport_size({'width':1440,'height':1100})
            page.evaluate('window.scrollTo(0,0)')
            page.screenshot(path=str(ROOT/'build/website-demos-1440.png'),full_page=False)
            page.set_viewport_size({'width':390,'height':950})
            page.screenshot(path=str(ROOT/'build/website-demos-390.png'),full_page=False)
        browser.close()
    print('PASS: five responsive widths, effect tabs/keyboard, 13-effect list, FAQ, one installer link, download guidance and no automatic media requests with reduced motion.')
    if '--media' in sys.argv:
        print('PASS: all four NVIDIA-hosted reference videos play and pause; external requests limited to the attributed NVIDIA media hosts.')


if __name__=='__main__':main()
