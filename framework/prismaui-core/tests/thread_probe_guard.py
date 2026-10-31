#!/usr/bin/env python3
from pathlib import Path

SRC = Path(__file__).resolve().parents[1] / 'src'
SITES = {
    'PrismaUI/WebRuntimePresent.cpp': 'if (ThreadObserver::Result::NewThread == PresentThreads().Observe(::GetCurrentThreadId())) logger::info("[ThreadProbe] Present',
    'PrismaUI/WebRuntimeFrame.cpp': 'if (ThreadObserver::Result::NewThread == ResizeThreads().Observe(::GetCurrentThreadId())) logger::info("[ThreadProbe] ResizeBuffers',
    'PrismaUI/WebRuntimeViews.cpp': 'logger::info("[ThreadProbe] WM_NCDESTROY',
}
USERS = {'PrismaUI/ThreadObserver.h', *SITES}


def fail(message):
    print(f'FAIL: {message}')
    raise SystemExit(1)


header = (SRC / 'PrismaUI/ThreadObserver.h').read_text(encoding='utf-8')
for banned in ('Windows.h', 'd3d11', 'dxgi', 'logger::', 'REX::'):
    if banned in header:
        fail(f'ThreadObserver.h must stay free of {banned}')

for relative, snippet in SITES.items():
    text = ' '.join((SRC / relative).read_text(encoding='utf-8').split())
    if 'ThreadObserver.h' not in text:
        fail(f'{relative} does not include ThreadObserver.h')
    if snippet not in text:
        fail(f'{relative}: the log-only probe call is missing or branches on the result')

views = ' '.join((SRC / 'PrismaUI/WebRuntimeViews.cpp').read_text(encoding='utf-8').split())
if 'PresentThreads().Contains(' not in views:
    fail('WebRuntimeViews.cpp no longer compares the WM_NCDESTROY thread with the Present threads')

for path in sorted(SRC.rglob('*')):
    if path.suffix not in ('.cpp', '.h', '.inl'):
        continue
    relative = path.relative_to(SRC).as_posix()
    text = path.read_text(encoding='utf-8', errors='replace')
    if ('PresentThreads()' in text or 'ResizeThreads()' in text) and relative not in USERS:
        fail(f'{relative} uses the thread probe; it must stay log-only in the three wired sites')

print('thread_probe_guard ok (probe is log-only and wired at Present, ResizeBuffers and WM_NCDESTROY)')
