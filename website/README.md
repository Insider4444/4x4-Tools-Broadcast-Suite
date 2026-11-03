# Download website

Buildless public landing page for 4x4Tools Broadcast Suite. Static HTML, CSS and JavaScript live in `dist`. Only original publisher artwork is used; the hero is explicitly a workflow illustration, not a claim of model output.

The main action downloads the single official Setup executable from GitHub's latest community release. No SDK/model selection, account form, analytics or NVIDIA credentials are part of the website.

The Sites project identity lives in `.openai/hosting.json`. Publishing uses an isolated checkout under ignored `build/site-work`; never publish the full plugin workspace, its SDKs or its `.env`. Copy only this directory's reviewed static files and manifest into that checkout.

For local development, run `python -m http.server 4173 --directory website/dist` from the project root, then visit `http://localhost:4173`. Use keyboard navigation, effect tabs, FAQ expansion and mobile widths when checking changes.

The primary public website is GitHub Pages, deployed only from `website/dist` by `.github/workflows/pages.yml`. The older Sites address redirects there; its identity is retained for redirect maintenance.
