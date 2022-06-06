/** Class that represents an information panel.
 *  Manages a list of pages that can be used to display different types of
 *  information on behalf of clients.
 */
class InfoPanel {
    /** Construct an InfoPanel object.
     *  @param menu is an html select menu used to select pages
     *  @param textArea is an html textArea where the pages are displayed
     */
    constructor(menu, textArea) {
        this.menu = menu;
        this.textArea = textArea;
        let s = this.menu.innerHTML;
        let options = s.split('<option value="');
        this.pageSet = {};
        for (let i = 1; i < options.length; i++) {
            let p = options[i].indexOf('">');
            if (p < 0) continue;
            let pageName = options[i].substr(0, p);
            this.pageSet[pageName] = { locked: true, getText: null };
        }
    }

    /** Register a callback for a page.
     *  @param pageName is used to identify the page
     *  @param getText is a function that can be called to retrieve the text
     *  to be displayed when the page option is selected
     */
    register(pageName, getText, context) {
        if (this.pageSet.hasOwnProperty(pageName)) {
            this.pageSet[pageName].getText = getText;
            this.pageSet[pageName].context = context;
        }
    }

	refresh(pageName) {
		if (pageName == this.currentPage) this.display(pageName);
	}

    /** Display the page specified in the menu.
     *  @param pageName specifies the page to be displayed; if not present
     *  the current menu value is used.
     */
    display(pageName=this.menu.value) {
        if (!this.pageSet.hasOwnProperty(pageName))
            pageName = this.menu.value;
        else
            this.menu.value = pageName;
		let page = this.pageSet[pageName];
        if (page.getText != null) {
            this.textArea.value = page.getText.call(page.context);
        } else {
            this.textArea.value = "no callback defined for this page";
		}
        this.textArea.readOnly = page.locked;
        this.textArea.style.backgroundColor =
                (page.locked ? "seashell" : "white");
    }

    /** Lock a specific page to prevent editing.
     * @param pageName identifies the page to be locked
     */
    lock(pageName) {
        if (this.pageSet.hasOwnProperty(pageName))
            this.pageSet[pageName].locked = true;
        this.display();
    }

    /** Unlock a specific page to allow editing.
     * @param pageName identifies the page to be locked
     */
    unlock(pageName) {
        if (this.pageSet.hasOwnProperty(pageName))
            this.pageSet[pageName].locked = false;
        this.display();
    }

	get currentPage() {
		return this.menu.value;
	}

    get pageText() {
        return this.textArea.value;
    }
};
