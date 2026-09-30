from pathlib import Path
page=Path('web/index.html').read_text()
Path('include/PortalPage.h').write_text('#pragma once\nconst char portalPage[] PROGMEM=R"MMPAGE('+page+')MMPAGE";\n')
