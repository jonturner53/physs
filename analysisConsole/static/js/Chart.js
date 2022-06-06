class AxisSpec {
    constructor(min=0, max=0, interval=0, ticks=6,
                precision=3, scaling="full", margin=0) {
        this.min = min; this.max = max;
        this.interval = interval; this.ticks = ticks;
        this.precision = precision; this.scaling = scaling;
        this.margin = margin;
    }
}

class ChartMarkup {
    constructor(leftHeader="", rightHeader="", label="", qa=-2,
                leftMargin=70, rightMargin=25, topMargin=25, botMargin=25) {
	    this.leftHeader = leftHeader; this.rightHeader = leftHeader;
	    this.label = label; this.qa = qa;
	    this.leftMargin = leftMargin; this.rightMargin = rightMargin;
	    this.topMargin = topMargin; this.botMargin = botMargin;
    }
}

/** Object used to display spectra.
 *  Has properties and methods that control how spectra are displayed.
 */
class Chart {
    /** @param canvas is the canvas where chart is to be drawn
     *  @param xycoord is a div element used to record position of mouse
     *  in chart area
     */
    constructor(canvas) {
        this.canvas = canvas;
        this.xycoord = xycoord;         ///< div element for mouse coordinates

        this.foreground = [];           ///< list of foreground curves
        this.background = [];           ///< list of background curves
        this.foregroundColors = [ "black", "blue", "green",
                                  "red", "darkMagenta", "teal" ];
                                        ///< colors for foreground curves
        this.showBG = false;

        this.chartwidth = 0;
        this.chartheight = 0;

        this.x = new AxisSpec();
        this.y = new AxisSpec();
        this.markup = new ChartMarkup();

        this.xscale = 1;                ///< scale factor for value-to-pix
        this.yscale = 1;
        this.x0 = 0; this.y0 = 0;       ///< graphics position of origin

        this.data = [];                 ///< array of vectors for current chart

        this.mouseUp = true;            ///< flag used for zoom control
        this.mousePoint = []            ///< point where mouse was pressed
        this.zoomBox = null;            ///< bounding box [xmin,ymin, xmax,ymax]
        this.zoomStack = [];            ///< stack of zoom boxes
    }

    /** Convert pixel position to a data value.
     *  @param xpix is a pixel offset in the x direction that falls
     *  within the chart area
     *  @return the x chart value corresponding to the position of xpix
     */
    xval(xpix) {
        return this.x.cmin + ((xpix - this.markup.leftMargin) / this.xscale); 
    }

    /** Convert pixel position to a data value.
     *  @param ypix is a pixel offset in the y direction that falls
     *  within the chart area
     *  @return the y chart value corresponding to the position of ypix
     */
    yval(ypix) {
        return this.y.cmax - ((ypix - this.markup.topMargin) / this.yscale); 
    }

    /** Convert an x data value to a pixel position.
     *  @param xval is an x chart value
     *  @return the x pixel offset corresponding to xval
     */
    xpix(xval) {
        return this.markup.leftMargin + ((xval - this.x.cmin) * this.xscale); 
    }

    /** Convert a y data value to a pixel position.
     *  @param yval is an y chart value
     *  @return the y pixel offset corresponding to yval
     */
    ypix(yval) {
        return this.markup.topMargin + ((this.y.cmax - yval) * this.yscale); 
    }

    /** Add a curve to the list of foreground curves.
     */
    addCurve(curve) {
        this.foreground.push(curve);
    }

    /** Remove all curves from the foreground list.
     */
    clearCurveSet() { this.foreground.length = 0; }

    /** Return the x ranges of the complete curve set.  */
    getXlimits() {
        let fg = this.foreground;
        if (fg.length == 0) return [0,0];

        let xlo = fg[0].xvec[0];
        let xhi = fg[0].xvec[fg[0].xvec.length-1];
        for (let i = 1; i < fg.length; i++) {
            xlo = Math.min(xlo, fg[i].xvec[0]);
            xhi = Math.max(xhi, fg[i].xvec[fg[i].xvec.length-1]);
        }

        if (!this.showBG) return [ xlo, xhi ];

        let bg = this.background;
        for (let i = 0; i < bg.length; i++) {
            xlo = Math.min(xlo, bg[i].xvec[0]);
            xhi = Math.max(xhi, bg[i].xvec[bg[i].xvec.length-1]);
        }
        return [ xlo, xhi ];
    }

    /** Return the y ranges of the complete curve set.  */
    getYlimits() {
        let fg = this.foreground;
        if (fg.length == 0) return [0,0];

        let ylo = lib.vectorMin(fg[0].yvec);
        let yhi = lib.vectorMax(fg[0].yvec);

        for (let i = 1; i < fg.length; i++) {
            ylo = Math.min(ylo, lib.vectorMin(fg[i].yvec));
            yhi = Math.max(yhi, lib.vectorMax(fg[i].yvec));
        }

        if (!this.showBG) return [ ylo, yhi ];

        let bg = this.background;
        for (let i = 0; i < bg.length; i++) {
            ylo = Math.min(ylo, lib.vectorMin(bg[i].yvec));
            yhi = Math.max(yhi, lib.vectorMax(bg[i].yvec));
        }
        return [ ylo, yhi ];
    }

    /** Add the current foreground curve into the background set. */
    addBGcurve(tag) {
		let n = parseInt(tag) - 1;
        if (this.foreground.length > n) {
            this.background.push(this.foreground[n]);
        }
        if (this.showBG) this.drawChart();
    }

    /** Remove the most recently added curve from the background set.  */
    dropBGcurve() {
        this.background.pop();
        if (this.showBG) this.drawChart();
    }

    showBGcurves() { this.showBG = true; this.drawChart(); }
    hideBGcurves() { this.showBG = false; this.drawChart(); }
    
    /** Get the string corresponding to the current chart data. */
    getDataString() {
        // create data string
        if (this.data.length == 0) return;
        let dataString = "";
        let sameX = true;
        for (let j = 0; j < this.data.length; j++) {
            if (this.data[j][0] != this.data[0][0])
                sameX = false;
        }
        if (sameX) {
            let n = this.data[0][0].length;
            for (let i = 0; i < n; i++) {
                dataString += lib.format(this.data[0][0][i], 4);
                for (let j = 0; j < this.data.length; j++) {
                    dataString += " ";
                    dataString += lib.format(this.data[j][1][i], 4);
                }
                dataString += "\n";
            }
        } else {
            for (let j = 0; j < this.data.length; j++) {
                if (j > 0) dataString += "\n";
                let n = this.data[j][0].length;
                for (let i = 0; i < n; i++) {
                    dataString += lib.format(this.data[j][0][i], 4);
                    dataString += " ";
                    dataString += lib.format(this.data[j][1][i], 4);
                    dataString += "\n";
                }
            }
        }
        return dataString;
    }
    
    /** Setup margins, draw background and axes.
     *  @param leftMargin specifies space for left margin (in pixels)
     *  @param qa is an optional quality assurance control variable;
     *  if present and >=-2, its value determines the color of the
     *  qa-indicator in the lower left corner of the chart area.
     */
    setup(x, y, markup) {
        Object.assign(this.x, x); Object.assign(this.y, y);
        Object.assign(this.markup, markup);

        this.chartwidth = this.canvas.width -
                          (markup.leftMargin + markup.rightMargin);
        this.chartheight = this.canvas.height -
                           (markup.botMargin + markup.topMargin);

        this.x0 = markup.leftMargin;
        this.y0 = this.canvas.height - markup.botMargin;
    }

    adjustAxisSpec(z, lo, hi) {
        if (this.zoomBox != null) {
            z.cmin = lo; z.cmax = hi;
        } else if (z.scaling != "none") {
            if (z.scaling == "full") {
                z.cmin = lo; z.cmax = hi;
            } else if (z.scaling == "expand") {
                z.cmin = Math.min(z.min, lo);
                z.cmax = Math.max(z.max, hi);
            } else { // z.scaling == "contract"
                z.cmin = Math.max(z.min, lo);
                z.cmax = Math.min(z.max, hi);
            }
            z.cmax += z.margin * (z.cmax - z.cmin);
        }
        if (z.cmin >= z.cmax) z.cmax = z.cmin + 1; 

        let firsttick = z.cmin; let delta = .9999 * (z.cmax - z.cmin) / z.ticks;
        if (this.zoomBox == null && z.scaling != "none" && z.interval != 0) {
            if (z.cmax - z.cmin < z.ticks * z.interval) {
                z.interval /= Math.ceil((z.interval * z.ticks) /
                                        (z.cmax - z.cmin));
            }
            firsttick = z.interval * Math.ceil(z.cmin/z.interval);
            delta = Math.floor((z.cmax - firsttick) / (z.ticks * z.interval))
                     * z.interval;
            if (delta == 0) delta = z.interval;
        }
        return [firsttick, delta];
    }

    drawChart() {
        // first establish limits based on zoombox or data
        let xlo, xhi, ylo, yhi;
        if (this.zoomBox != null) {
            xlo = this.zoomBox[0]; xhi = this.zoomBox[2];
            ylo = this.zoomBox[1]; yhi = this.zoomBox[3];
        } else {
            [xlo, xhi] = this.getXlimits();
            if (this.x.scaling != "none") {
                if (this.x.min <  0) 
                    this.x.min = Math.max(0, xhi + this.x.min);
                if (this.x.max <= 0)
                    this.x.max = Math.max(this.x.min, this.xhi + this.x.max);
            }
            [ylo, yhi] = this.getYlimits();
        }
        this.x.cmin = this.x.min; this.x.cmax = this.x.max;
        this.y.cmin = this.y.min; this.y.cmax = this.y.max;
        
        // now adjust the axes parameters and get value at first tick mark
        // and actual separation between tick marks
        let [xfirsttick, xdelta] = this.adjustAxisSpec(this.x, xlo, xhi);
        let [yfirsttick, ydelta] = this.adjustAxisSpec(this.y, ylo, yhi);

        this.xscale = this.chartwidth  / (this.x.cmax - this.x.cmin);
        this.yscale = this.chartheight / (this.y.cmax - this.y.cmin);

        // draw chart framework
        let ctx = this.canvas.getContext("2d");
        ctx.font = "12px Verdana";

        ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        ctx.beginPath();
        ctx.rect(this.markup.leftMargin, this.markup.topMargin,
                 this.chartwidth, this.chartheight);
        ctx.fillStyle = "#d0e0f0";
        ctx.fill(); 

        ctx.beginPath();
        ctx.strokeStyle = "black";
        ctx.fillStyle = "black";
        ctx.lineWidth = .8;
        ctx.moveTo(this.x0, this.y0 - this.chartheight);
        ctx.lineTo(this.canvas.width - this.markup.rightMargin,
                   this.y0 - this.chartheight);
        ctx.lineTo(this.canvas.width - this.markup.rightMargin, this.y0);
        ctx.stroke();

        ctx.beginPath();
        ctx.lineWidth = 2;
        ctx.moveTo(this.x0, this.y0 - this.chartheight);
        ctx.lineTo(this.x0, this.y0);
        ctx.lineTo(this.canvas.width - this.markup.rightMargin, this.y0);
        ctx.stroke();

        // draw axis tkcks and labels
        ctx.beginPath();
        ctx.strokeStyle = "black";
        ctx.fillStyle = "black";
        ctx.lineWidth = 1;
        ctx.textAlign = "center";

        let x = xfirsttick;
        while (x <= this.x.cmax) {
            let xpix = this.xpix(x);
            ctx.moveTo(xpix, this.y0 - 10);
            ctx.lineTo(xpix, this.y0);
            ctx.fillText(lib.format(x, this.x.precision), xpix, this.y0 + 15);
            x += xdelta;
        }
    
        ctx.textAlign = "right";
        let y = yfirsttick; 
        while (y <= this.y.cmax) {
            let ypix = this.ypix(y);
            ctx.moveTo(this.x0, ypix);
            ctx.lineTo(this.x0 + 10, ypix);
            ctx.moveTo(this.canvas.width - this.rightMargin, ypix);
            ctx.lineTo(this.canvas.width - (this.rightMargin + 10), ypix);
            ctx.fillText(lib.format(y, this.y.precision), this.x0 -5, ypix + 2);
            y += ydelta;
        }
        ctx.stroke();

        // add markup
        ctx.strokeStyle = "black";
        ctx.fillStyle = "black";
        ctx.lineWidth = 1;

        ctx.textAlign = "left";
        ctx.fillText(this.markup.leftHeader,
                     this.x0, this.y0 - (this.chartheight+10));
        ctx.textAlign = "right";
        ctx.fillText(this.markup.rightHeader,
                     this.x0 + this.chartwidth,
                     this.y0 - (this.chartheight+10));
        if (typeof this.markup.label == "string") {
            ctx.fillText(this.markup.label,
                         this.x0 + this.chartwidth - 15,
                         (this.y0 - this.chartheight) + 13);
        } else {
            for (let i = 0; i < this.markup.label.length; i++) {
                ctx.fillText(this.markup.label[i],
                             this.x0 + this.chartwidth - 15,
                             (this.y0 - this.chartheight) + 13 + i*15);
            }
        }

        if (this.markup.qa != -2) {
            if (this.markup.qa == -1) ctx.fillStyle = "red";
            else if (this.markup.qa == 0) ctx.fillStyle = "yellow";
            else ctx.fillStyle = "green";

            ctx.rect(5, this.canvas.height-15, 10, 10);
            ctx.stroke(); ctx.fill();
        }

        this.drawCurveSet();
    }

    drawCurveSet() {
        this.data.length = 0;
        for (let i = 0; i < this.foreground.length; i++) {
            let color = this.foreground[i].color;
            if (color == "auto")
                color = this.foregroundColors[
                        i % this.foregroundColors.length];
            this.drawCurve(this.foreground[i].xvec,
                            this.foreground[i].yvec,
                            color, this.foreground[i].style);
        }
        if (!this.showBG) return;
        for (let i = 0; i < this.background.length; i++) {
            this.drawCurve(this.background[i].xvec,
                            this.background[i].yvec, "gray", "solid");
        }
    }

    /** Draw a single curve.
     *  @param xv is a vector of x-values
     *  @param yv is a vector of y-values
     *  @param color is a string specifying the color for the curve
     */
    drawCurve(xv, yv, color="auto", style="solid") {
        let ctx = this.canvas.getContext("2d");
        let i = 0;
        while (i < xv.length) {
            if (xv[i] >= this.x.cmin && xv[i] <= this.x.cmax)
                break;
            i++;
        }
        if (i == xv.length) return;
        for (i = i+1; i < xv.length && xv[i] <= this.x.cmax; i++) {
            // handle clipping
            if ((yv[i-1] < this.y.cmin && yv[i] < this.y.cmin) ||
                (yv[i-1] > this.y.cmax && yv[i] > this.y.cmax))
                continue;
            let x = xv[i]; let y = yv[i];
            let xx = xv[i-1]; let yy = yv[i-1];
            let s = Math.abs((yv[i]-yv[i-1]) / (xv[i]-xv[i-1]));
            if (yv[i-1] < this.y.cmin) {
                xx = xv[i-1] + (this.y.cmin-yv[i-1])/s;
                yy = this.y.cmin;
            } else if (yv[i-1] > this.y.cmax) {
                xx = xv[i-1] + (yv[i-1]-this.y.cmax)/s;
                yy = this.y.cmax;
            }
            if (yv[i] < this.y.cmin) {
                x = xv[i] - (this.y.cmin-yv[i])/s;
                y = this.y.cmin;
            } else if (yv[i] > this.y.cmax) {
                x = xv[i] - (yv[i]-this.y.cmax)/s;
                y = this.y.cmax;
            }

            ctx.beginPath();
            ctx.moveTo(this.xpix(xx), this.ypix(yy));
            ctx.lineTo(this.xpix(x), this.ypix(y));
            ctx.strokeStyle = (typeof color == "string" ? color : color[i]);
            if (style == "dashed") ctx.setLineDash([6,10]);
            else if (style == "dotted") ctx.setLineDash([2,6]);
            else ctx.setLineDash([]);
            ctx.lineWidth = .75;
            ctx.stroke();
            ctx.setLineDash([]);
        }

        this.data.push([xv, yv]);
    }

    clearZoom() {
        this.mouseUp = true; this.zoomBox = null;
        this.zoomStack.length = 0;
    }

    inZoomRange(x, y) {
        // allow small excursion outside range to enable zooming near borders
        return (x >= this.markup.leftMargin - 2 &&
                x <= 2 + this.canvas.width - this.markup.rightMargin &&
                y >= this.markup.topMargin - 2 &&
                y <= 2 + this.canvas.height - this.markup.botMargin);
    }

    mousePress(event) {
        let x = event.offsetX;
        let y = event.offsetY;
        if (!this.inZoomRange(x,y)) return;
        this.mouseUp = false;
        this.mousePoint = [x, y];
    }

    mouseRelease(event) {
        if (this.mouseUp) return;
        this.mouseUp = true;

        let x = event.offsetX;
        let y = event.offsetY;
        if (!this.inZoomRange(x,y)) return;

        let x0 = this.mousePoint[0];
        let y0 = this.mousePoint[1];
        if (x == x0 && y == y0) { // simple click
            if (this.zoomStack.length == 0) return;
            this.zoomStack.pop();
            if (this.zoomStack.length != 0) 
                this.zoomBox = this.zoomStack[this.zoomStack.length-1];
            else
                this.zoomBox = null;
            this.drawChart();
            return;
        }

        this.zoomBox = [ this.xval(Math.min(x, x0)),
                         this.yval(Math.max(y, y0)),
                         this.xval(Math.max(x, x0)),
                         this.yval(Math.min(y, y0)) ];

        this.zoomStack.push(this.zoomBox);
        this.drawChart();
    }

    mouseMove(event) {
        let x = event.offsetX;
        let y = event.offsetY;
        if (!this.inZoomRange(x,y)) {
            main.reportMouse(0,0,true);
            this.canvas.style.cursor = "crosshair";
            return;
        }
        main.reportMouse(this.xval(x), this.yval(y));
        this.canvas.style.cursor = "crosshair";
        if (this.mouseUp) return;

        this.drawChart();
        //main.chartMod();
        
        let x0 = this.mousePoint[0];
        let y0 = this.mousePoint[1];
        let ctx = this.canvas.getContext("2d");
        ctx.strokeStyle = "black";
        ctx.lineWidth = 1;
        ctx.strokeRect(Math.min(x,x0), Math.min(y,y0),
                       Math.abs(x-x0), Math.abs(y-y0));
    }

    mouseOut() { this.mouseUp = true; }
};

