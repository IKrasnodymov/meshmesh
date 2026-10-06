from pathlib import Path
page=Path('web/index.html').read_text()
target=Path('include/PortalPage.h')
generated='#pragma once\nconst char portalPage[] PROGMEM=R"MMPAGE('+page+')MMPAGE";\n'
if not target.exists() or target.read_text()!=generated:
    target.write_text(generated)
