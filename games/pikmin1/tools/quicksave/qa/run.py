import json, os, plistlib, re, shlex, shutil, signal, subprocess, sys, time
from pathlib import Path

import argparse

PARSER = argparse.ArgumentParser(description="Isolated quick-save QA runs; existing output directories are never overwritten.")
PARSER.add_argument('cases',type=Path)
PARSER.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[5])
PARSER.add_argument('--out',type=Path,required=True)
PARSER.add_argument('--exe',type=Path,required=True)
PARSER.add_argument('--fixtures',type=Path,default=Path.home()/'.local/share/pikmin-native',help='Contains assets/, save/ and pikmin_settings.conf; private files are copied, never edited.')
OPTIONS = PARSER.parse_args()
ROOT = OPTIONS.out.resolve()
ROOT.mkdir(parents=True,exist_ok=True)
W = OPTIONS.repo.resolve()
REAL = OPTIONS.fixtures.resolve()
SCRIPTS = Path(__file__).resolve().parent/'scripts'

def expand(value):
    if isinstance(value,dict): return {k:expand(v) for k,v in value.items()}
    if isinstance(value,list): return [expand(v) for v in value]
    if isinstance(value,str):
        for key,path in dict(out=ROOT,repo=W,exe=OPTIONS.exe.resolve(),scripts=SCRIPTS).items():
            value=value.replace('{'+key+'}',str(path))
    return value

def run(c):
    d = ROOT / c['name']
    d.mkdir() # preserve earlier runs
    (d/'assets').symlink_to(REAL/'assets')
    source = Path(c.get('savefrom', REAL/'save'))
    for card in ('card0','card1'):
        if (source/card).exists(): shutil.copytree(source/card,d/'save'/card)
    if c.get('copy_quicksaves') and (source/'quicksave').exists():
        shutil.copytree(source/'quicksave',d/'save/quicksave')
    text = (REAL/'pikmin_settings.conf').read_text()
    cfg = dict(windowWidth=1280,windowHeight=720,windowMode=0,renderScale=1,vsync=0,quickSave=1)
    cfg.update(c.get('cfg',{}))
    for k,v in cfg.items():
        pattern = r'^'+re.escape(k)+r' = .*$'
        if re.search(pattern,text,re.M): text = re.sub(pattern,f'{k} = {v}',text,flags=re.M)
        else: text += f'\n{k} = {v}\n'
    (d/'pikmin_settings.conf').write_text(text)
    (d/'settings.start.conf').write_text(text)
    env = {k:v for k,v in os.environ.items() if not k.startswith(('PIKMIN_','NECTAR_','SDL_'))}
    env.update(NECTAR_SAVE_DIR=str(d/'save'), XDG_DATA_HOME=str(d/'data'), SDL_AUDIODRIVER='dummy',
               PIKMIN_STATE_HASH_LOG=str(d/'hashes.txt'), PIKMIN_NETPLAY_EXIT_AFTER_TICKS=str(c.get('ticks',1500)),
               PIKMIN_NETPLAY_SEED='0')
    if c.get('unthrottled',True): env['PIKMIN_NETPLAY_UNTHROTTLED']='1'
    env.update({k:str(v) for k,v in c.get('env',{}).items()})
    args = [str(c.get('exe',OPTIONS.exe.resolve()))]
    if c.get('mode')=='vs': args += ['--vs-map',str(W/'games/pikmin1/maps/vs/impact_arena/map.json')]
    if c.get('mode')=='coop': args += ['--coop-direct']
    args += c.get('args',[])
    (d/'case.json').write_text(json.dumps(dict(c,args=args,environment={k:v for k,v in env.items() if k.startswith(('PIKMIN_','NECTAR_','SDL_','XDG_'))}),indent=2))
    if c.get('gui'):
        # One identity across scenarios so a saved app approval remains useful.
        app = ROOT / 'QA-ui-clean.app'
        mac = app/'Contents/MacOS'
        mac.mkdir(parents=True,exist_ok=True)
        shutil.copy(args[0],mac/'nectar')
        (app/'Contents/Info.plist').write_bytes(plistlib.dumps(dict(CFBundleName='QA-ui-clean',CFBundleIdentifier='local.pikmin.qa.ui-clean',CFBundleExecutable='run',CFBundlePackageType='APPL',CFBundleVersion='1',NSHighResolutionCapable=True)))
        script = '#!/bin/bash\ncd '+shlex.quote(str(d))+' || exit 1\n'
        for k,v in env.items():
            if k.startswith(('PIKMIN_','NECTAR_','SDL_','XDG_')): script += 'export '+k+'='+shlex.quote(v)+'\n'
        script += ' '.join(map(shlex.quote,[str(mac/'nectar')]+args[1:]))+' > '+shlex.quote(str(d/'native.log'))+' 2>&1 &\n'
        script += 'game_pid=$!\necho "$game_pid" > '+shlex.quote(str(d/'game.pid'))+'\nwait "$game_pid"\n'
        script += 'echo $? > '+shlex.quote(str(d/'exitcode'))+'\n'
        (mac/'run').write_text(script)
        (mac/'run').chmod(0o755)
        args = ['open','-n','-W',str(app)]
    start = time.monotonic()
    with (d/'native.log').open('w') as log:
        proc = subprocess.Popen(args,cwd=d,env=env,stdout=log,stderr=subprocess.STDOUT)
        (d/'pid').write_text(str(proc.pid))
        print(f"START {c['name']} pid={proc.pid}",flush=True)
        try: rc = proc.wait(timeout=c.get('timeout',300))
        except subprocess.TimeoutExpired:
            if c.get('gui') and (d/'game.pid').exists():
                game_pid = int((d/'game.pid').read_text())
                command = subprocess.run(['ps','-p',str(game_pid),'-o','command='],capture_output=True,text=True).stdout
                if str(mac/'nectar') in command:
                    os.kill(game_pid,signal.SIGTERM)
            proc.terminate()
            try: proc.wait(timeout=5)
            except subprocess.TimeoutExpired: proc.kill(); proc.wait()
            rc = 'TIMEOUT'
    if c.get('gui') and (d/'exitcode').exists(): rc = int((d/'exitcode').read_text())
    result = dict(name=c['name'],exit=rc,seconds=round(time.monotonic()-start,2))
    (d/'result.json').write_text(json.dumps(result))
    print(json.dumps(result),flush=True)
    for line in (d/'native.log').read_text(errors='replace').splitlines():
        if any(s in line for s in ('[quicksave]','[QS-SET]','[QS-SUNSET]','[QS-PLUCK]','Internal render resolution')):
            print(line,flush=True)

for case in expand(json.loads(OPTIONS.cases.read_text())): run(case)
