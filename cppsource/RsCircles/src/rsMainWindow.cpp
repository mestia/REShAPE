#include "rsMainWindow.h"

std::string rsImageView_VERSION = "1.0.3 (2023-01-10)";

rsMainWindow* rsMainWindow::TheApp = NULL;

static QColor niColor(0xFF, 0x00, 0xFF, 0xFF);
static QColor hiColor(0xFF, 0xAA, 0xFF, 0xFF);
static QColor noColor(0x00, 0xFF, 0x00, 0xFF);
static QColor hoColor(0xAA, 0xFF, 0xAA, 0xFF);

static struct rsCellSizeDescr {
	int vsz;
	const char* label;
} cell_sizes[] = {
	{1, "Tiny (1)"},
	{2, "X-Small (2)"},
	{3, "Small (3)"},
	{5, "Medium (5)"},
	{7, "Large (7)"},
	{9, "X-Large (9)"}
};
static const int n_cell_sizes = sizeof cell_sizes / sizeof cell_sizes[0];

static int closest_cell_size_index(double sz)
{
	int cidx = -1;
	double bsf = 1000.;
	for (int i = 0; i < n_cell_sizes; i++) {
		double dist = fabs(sz - double(cell_sizes[i].vsz));
		if (cidx < 0 || dist < bsf) {
			cidx = i;
			bsf = dist;
		}
	}
	return cidx;
}

// Constructor
rsMainWindow::rsMainWindow()
	: innerCir(0, 0, 0, niColor, hiColor), outerCir(0, 0, 0, noColor, hoColor),
	innerPoly(0, 0, 0, niColor, hiColor), outerPoly(0, 0, 0, noColor, hoColor)
{
	dfltLimits.resize(numVisualParams + 2);
	curLimits.resize(numVisualParams + 2);

	homeDir = QDir(QDir::home().filePath(".reshape"));
	if (!homeDir.exists()) homeDir.mkpath(".");
	stateFile = QFileInfo(homeDir, QString("RsCirclesState.json"));

	QScreen *screen = QGuiApplication::primaryScreen();
	QRect  screenGeometry = screen->geometry();
	screen_height = screenGeometry.height();
	screen_width = screenGeometry.width();

	setWindowTitle(tr("REShAPE Circles ver. ") + QString(rsImageView_VERSION.c_str()));
	setMinimumSize(screen_width / 3, screen_height / 3);
	resize(screen_width * 70 / 100, screen_height * 75 / 100);
	move(screen_width / 6, screen_height / 8);

	lutMgr = new rsLUT();
	lutMgr->loadPalettes();

	img_width = img_height = 2500;
	scale_to_img = 1.;
	max_rad = 0.;
	imgView = new rsImageViewQt();
	imgView->SetGlasbeyPalette(lutMgr->GetGlasbey());
	imgView->SetLegendWidth(screen_height * 26 / 100);

	setMouseTracking(true);

	createActions();
	createMenus();

	createView();

	TheApp = this;
	srcDir = QDir::home();
	tgtDir = QDir::home();
	loadState();

	setAcceptDrops(true);
	connect(this, SIGNAL(triggerOpenFile(const char *)), this, SLOT(handleOpenFile(const char *)));
}

rsMainWindow::~rsMainWindow()
{
	delete imgView;
}

void rsMainWindow::createActions()
{
	openDataAct = new QAction(tr("&Open..."), this);
	openDataAct->setIcon(QIcon(":open.png"));
	openDataAct->setShortcuts(QKeySequence::Open);
	openDataAct->setToolTip(tr("Open REShAPE CSV (Ctrl+O)"));
	connect(openDataAct, SIGNAL(triggered()), this, SLOT(openImageData()));

	saveDataAct = new QAction(tr("&Save As..."), this);
	saveDataAct->setIcon(QIcon(":saveas.png"));
	saveDataAct->setToolTip(tr("Save Selected Data"));
	connect(saveDataAct, SIGNAL(triggered()), this, SLOT(saveImageData()));

	resetAct = new QAction(tr("Reset Selection"), this);
	resetAct->setIcon(QIcon(":reset.png"));
	resetAct->setToolTip(tr("Reset Circle Selection"));
	connect(resetAct, SIGNAL(triggered()), this, SLOT(resetSelectors()));

	quitAct = new QAction(tr("&Close"), this);
	quitAct->setShortcuts(QKeySequence::Quit);
	quitAct->setToolTip(tr("Close the program (Alt+F4)"));
	connect(quitAct, SIGNAL(triggered()), this, SLOT(quit()));

	resetAct->setEnabled(false);
}

void rsMainWindow::createMenus()
{
	fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(openDataAct);
	fileMenu->addAction(saveDataAct);
	fileMenu->addAction(quitAct);

	actionMenu = menuBar()->addMenu(tr("&Action"));
	actionMenu->addAction(resetAct);
}

void rsMainWindow::createView()
{
	int i;
	QWidget *centralwidget = new QWidget();
	centralwidget->setAttribute(Qt::WA_DeleteOnClose, true);
	centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
	setCentralWidget(centralwidget);

	imageWidget = imgView->GetImageWidget(centralwidget);

	ctrlLayout = new QGridLayout();
	ctrlLayout->setMargin(5);
	ctrlLayout->setSpacing(15);

	paramBox = new QGroupBox(tr("Feature Settings"));
	paramLayout = new QGridLayout();
	paramLayout->setColumnStretch(0, 0);
	paramLayout->setColumnStretch(1, 10);
	paramLayout->setColumnStretch(2, 0);
	paramLayout->setColumnStretch(3, 10);
	paramBox->setLayout(paramLayout);
	ctrlLayout->addWidget(paramBox, 0, 0, 1, 2);

	ctrlLabel = new QLabel(tr("Feature:"));
	//ctrlLabel->setStyleSheet(tr("QLabel{height: 3em;}"));
	paramLayout->addWidget(ctrlLabel, 0, 0, Qt::AlignRight);
	cbParam = new QComboBox();
	cbParam->setStyleSheet(tr("QComboBox {height: 1.5em;}"));
	cbParam->setEditable(false);
	cbParam->addItem("Coordinates");
	cbParam->addItem("Neighbors");
	for (i = 0; i < numVisualParams; i++) {
		cbParam->addItem(visualParams[i].text);
	}
	paramLayout->addWidget(cbParam, 0, 1, 1, 3);

	cellSizeLabel = new QLabel(tr("Cell Size:"));
	paramLayout->addWidget(cellSizeLabel, 2, 0, Qt::AlignRight);
	cbCellSize = new QComboBox();
	cbCellSize->setStyleSheet(tr("QComboBox {height: 1.5em;}"));
	for (i = 0; i < n_cell_sizes; i++) {
		cbCellSize->addItem(cell_sizes[i].label);
	}
	cbCellSize->setCurrentIndex(2);
	paramLayout->addWidget(cbCellSize, 2, 1, 1, 3);

	lbOpacity = new QLabel(tr("Opacity:"));
	slOpacity = new QSlider(Qt::Horizontal);
	slOpacity->setRange(10, 100);
	slOpacity->setValue(100);
	paramLayout->addWidget(lbOpacity, 3, 0, Qt::AlignRight);
	paramLayout->addWidget(slOpacity, 3, 1, 1, 3);

	lutLabel = new QLabel(tr("LUT:"));
	paramLayout->addWidget(lutLabel, 4, 0, Qt::AlignRight);
	cbLut = new QComboBox();
	cbLut->setStyleSheet(tr("QComboBox {height: 1.5em;}"));
	cbLut->setEditable(false);
	for (i = 0; i < lutMgr->GetNumPalettes(); i++) {
		cbLut->addItem(lutMgr->GetPaletteAt(i).GetName().c_str());
	}
	paramLayout->addWidget(cbLut, 4, 1, 1, 3);

	legendLabel = new QLabel();
	QPixmap pm = QPixmap::fromImage(*imgView->GetLegend());
	legendLabel->setPixmap(pm);
	legendLabel->resize(pm.size());
	paramLayout->addWidget(legendLabel, 5, 0, 1, 4);

	lbMin = new QLabel(tr("Limits:"));
	paramLayout->addWidget(lbMin, 6, 0, Qt::AlignRight);
	txMin = new QLineEdit();
	txMin->setStyleSheet(tr("QLineEdit {width: 6em;}"));
	paramLayout->addWidget(txMin, 6, 1);
	lbMax = new QLabel(tr(".."));
	paramLayout->addWidget(lbMax, 6, 2);
	txMax = new QLineEdit();
	txMax->setStyleSheet(tr("QLineEdit {width: 6em;}"));
	paramLayout->addWidget(txMax, 6, 3);

	updButton = new QPushButton(tr("Update"));
	paramLayout->addWidget(updButton, 7, 2, 1, 2);

	inLabel = new QLabel(tr("Inner:"));
	ctrlLayout->addWidget(inLabel, 3, 0, Qt::AlignRight);
	cbInner = new QComboBox();
	cbInner->setStyleSheet(tr("QComboBox {height: 1.5em;}"));
	cbInner->addItem(tr("Circle"));
	cbInner->addItem(tr("Polygon"));
	ctrlLayout->addWidget(cbInner, 3, 1);

	outLabel = new QLabel(tr("Outer:"));
	ctrlLayout->addWidget(outLabel, 4, 0, Qt::AlignRight);
	cbOuter = new QComboBox();
	cbOuter->setStyleSheet(tr("QComboBox {height: 1.5em;}"));
	cbOuter->addItem(tr("Circle"));
	cbOuter->addItem(tr("Polygon"));
	ctrlLayout->addWidget(cbOuter, 4, 1);

	arcBox = new QGroupBox(tr("Area Info"));
	ctrlLayout->addWidget(arcBox, 5, 0, 1, 2);
	arcLayout = new QGridLayout();
	arcBox->setLayout(arcLayout);
	arcText = new QTextEdit();
	arcText->setReadOnly(true);
	arcText->setFixedHeight(screen_height/6);
	arcLayout->addWidget(arcText, 0, 0);
	arcButton = new QPushButton(tr("Calculate Areas"));
	arcLayout->addWidget(arcButton, 1, 0);

	QWidget *bottomWidget = new QWidget();
	bottomWidget->setAttribute(Qt::WA_DeleteOnClose, true);
	ctrlLayout->addWidget(bottomWidget, 6, 0, 1, 2);

	ctrlLayout->setColumnStretch(1, 100);
	ctrlLayout->setRowStretch(0, 0);
	ctrlLayout->setRowStretch(6, 10);

	viewLayout = new QGridLayout(centralwidget);
	viewLayout->setObjectName(QString::fromUtf8("viewLayout"));
	viewLayout->addLayout(ctrlLayout, 0, 0);
	viewLayout->addWidget(imageWidget, 0, 1);
	viewLayout->setColumnStretch(0, 0);
	viewLayout->setColumnStretch(1, 100);

	loadLimits();

	connect(cbParam, SIGNAL(currentIndexChanged(int)), this, SLOT(visualParamChanged(int)));
	connect(cbCellSize, SIGNAL(currentIndexChanged(int)), this, SLOT(cellSizeChanged(int)));
	connect(slOpacity, SIGNAL(sliderReleased()), this, SLOT(opacitySliderReleased()));
	connect(cbLut, SIGNAL(currentIndexChanged(int)), this, SLOT(lutChanged(int)));
	cbLut->setCurrentIndex(lutMgr->FindPalette("Thermal"));

	connect(cbInner, SIGNAL(currentIndexChanged(int)), this, SLOT(innerChanged(int)));
	connect(cbOuter, SIGNAL(currentIndexChanged(int)), this, SLOT(outerChanged(int)));

	connect(arcButton, SIGNAL(clicked()), this, SLOT(arcButtonClicked()));
	connect(updButton, SIGNAL(clicked()), this, SLOT(updButtonClicked()));

	centralwidget->setMouseTracking(true);
}

void rsMainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	event->acceptProposedAction();
}

void rsMainWindow::dropEvent(QDropEvent *e)
{
	for (int i = 0; i < e->mimeData()->urls().size(); i++) {
		QString fn = e->mimeData()->urls()[i].toLocalFile();
		if (fn.endsWith(tr(".csv"))) {
			// std::cout << "trigger open " << fn.toStdString() << std::endl;
			readCsvFile(fn.toStdString().c_str());
			//emit triggerOpenFile(fn.toStdString().c_str());
			break;
		}
	}
}

void rsMainWindow::handleOpenFile(const char *fn)
{
	// std::cout << "handle open " << fn << std::endl;
	readCsvFile(fn);
}

void rsMainWindow::visualParamChanged(int idx_par)
{
	// std::cout << "idx_par = " << std::to_string(idx_par) << std::endl;
	loadLimits();

	int vsz = cell_sizes[cbCellSize->currentIndex()].vsz;
	imgView->SetParticleSize(vsz);
	double opacity = slOpacity->value() * 0.01;
	imgView->SetOpacity(opacity);

	rsParamLimits& cpl = curLimits[idx_par];
	imgView->SetVisualParameter(idx_par, cpl.vmin, cpl.vmax);
	imgView->ResetView(false);
	legendLabel->setPixmap(QPixmap::fromImage(*imgView->GetLegend()));
}

void rsMainWindow::cellSizeChanged(int idx)
{
	if (_busy) return;

	int vsz = cell_sizes[cbCellSize->currentIndex()].vsz;
	imgView->SetParticleSize(vsz);
	double opacity = slOpacity->value() * 0.01;
	imgView->SetOpacity(opacity);

	loadLimits();
	imgView->reFillParticles();
	imgView->ResetView(false);
}
void rsMainWindow::opacitySliderReleased()
{
	loadLimits();
	cellSizeChanged(cbCellSize->currentIndex());
}

void rsMainWindow::lutChanged(int idx_lut)
{
	loadLimits();
	imgView->SetPalette(&lutMgr->GetPaletteAt(idx_lut));
	imgView->ResetView(false);
	legendLabel->setPixmap(QPixmap::fromImage(*imgView->GetLegend()));
}

void rsMainWindow::closeEvent(QCloseEvent *e)
{
	saveState();
	QMainWindow::closeEvent(e);
}

void rsMainWindow::quit()
{
	close();
}

void rsMainWindow::openImageData()
{
	QFileDialog dialog(this);
	dialog.setFileMode(QFileDialog::ExistingFile);
	dialog.setNameFilter(tr("REShAPE CSV files (*.csv)"));
	dialog.setWindowTitle("Open CSV Data file");
	dialog.restoreState(srcDialogState);
	dialog.setDirectory(srcDir);

	QStringList fileNames;
	if (dialog.exec())
	{
		fileNames = dialog.selectedFiles();
	}
	if (!fileNames.isEmpty()) {
		srcDir = dialog.directory();
		srcDialogState = dialog.saveState();
		saveState();

		QString fn = fileNames[0];
		if (fn.endsWith(tr(".csv")) ) {
			// std::cout << fn.toStdString() << std::endl;
			fileName = fn;
			readCsvFile(fn.toStdString().c_str());
		}
	}
}

void rsMainWindow::saveImageData()
{
	if (particles.size() == 0) return;

	QFileInfo finfo(fileName);
	QString fn = finfo.baseName();
	if (fn.endsWith("_Data")) {
		fn.resize(fn.size() - 5);
	}
	else if (fn.endsWith("_Circles")) {
		fn.resize(fn.size() - 7);
	}
	fn.append("_Circles.csv");

	QFileDialog dialog(this);
	dialog.setFileMode(QFileDialog::AnyFile);
	dialog.setAcceptMode(QFileDialog::AcceptSave);
	dialog.setWindowTitle("Save Selection");
	dialog.selectFile(fn);
	dialog.selectNameFilter("REShAPE CSV files (*.csv)");
	dialog.restoreState(tgtDialogState);
	dialog.setDirectory(tgtDir);

	QStringList fileNames;
	if (dialog.exec())
	{
		fileNames = dialog.selectedFiles();
	}
	if (!fileNames.isEmpty()) {
		tgtDir = dialog.directory();
		tgtDialogState = dialog.saveState();
		saveState();

		fn = fileNames[0];
		writeCsvFile(fn.toStdString().c_str());
	}
}

void rsMainWindow::updateParticleLimits()
{
	for (int j = 0; size_t(j) < dfltLimits.size(); j++) {
		rsParamLimits& pl = dfltLimits[j];
		pl.vmax = pl.vmin = NaN;
	}
	for (rsParticle& pt : particles) {
		for (int i = 0; i < numVisualParams; i++) {
			size_t par_off = visualParams[i].off;
			double v = *pt.parPtr(par_off);
			if (std::isnan(v)) continue;
			rsParamLimits& pl = dfltLimits[i + 2];
			if (std::isnan(pl.vmin)) {
				pl.vmin = pl.vmax = v;
				continue;
			}
			if (pl.vmin > v) pl.vmin = v;
			if (pl.vmax < v) pl.vmax = v;
		}
	}
	for (int j = 0; size_t(j) < dfltLimits.size(); j++) {
		rsParamLimits& pl = dfltLimits[j];
		if (!std::isnan(pl.vmin)) {
			if (pl.vmin == pl.vmax) pl.vmax = pl.vmin + 1.;
		}
		curLimits[j] = pl;
	}
}

void rsMainWindow::validateLimits()
{
	int idx = cbParam->currentIndex();
	if (idx < 2) {
		txMin->setText(tr(""));
		txMin->setEnabled(false);
		txMax->setText(tr(""));
		txMax->setEnabled(false);
		return;
	}
	txMin->setEnabled(true);
	txMax->setEnabled(true);

	rsParamLimits& dpl = dfltLimits[idx];
	rsParamLimits& cpl = curLimits[idx];

	bool ok = false;
	double vmin = txMin->text().toDouble(&ok);
	if (ok) {
		cpl.vmin = vmin;
	}
	else {
		cpl.vmin = dpl.vmin;
	}
	double vmax = txMax->text().toDouble(&ok);
	if (ok) {
		cpl.vmax = vmax;
	}
	else {
		cpl.vmax = dpl.vmax;
	}
	QString txt;
	if (!std::isnan(cpl.vmin))
		txt = QString("%1").arg(cpl.vmin, 0, 'F', 2);
	txMin->setText(txt);
	if (!std::isnan(cpl.vmax)) {
		if (cpl.vmin == cpl.vmax) cpl.vmax = cpl.vmin + 1.;
		txt = QString("%1").arg(cpl.vmax, 0, 'F', 2);
	}
	else txt = "";
	txMax->setText(txt);
}

void rsMainWindow::loadLimits()
{
	txMin->setText(tr(""));
	txMax->setText(tr(""));
	int idx = cbParam->currentIndex();
	if (idx < 2) {
		txMin->setEnabled(false);
		txMax->setEnabled(false);
		return;
	}
	txMin->setEnabled(true);
	txMax->setEnabled(true);

	rsParamLimits& cpl = curLimits[idx];
	if (!std::isnan(cpl.vmin)) {
		QString txt = QString("%1").arg(cpl.vmin, 0, 'F', 2);
		txMin->setText(txt);
	}
	if (!std::isnan(cpl.vmax)) {
		QString txt = QString("%1").arg(cpl.vmax, 0, 'F', 2);
		txMax->setText(txt);
	}
}

void rsMainWindow::updButtonClicked()
{
	validateLimits();
	int idx_par = cbParam->currentIndex();
	rsParamLimits& cpl = curLimits[idx_par];
	imgView->SetVisualParameter(idx_par, cpl.vmin, cpl.vmax);
	imgView->ResetView(false);
	legendLabel->setPixmap(QPixmap::fromImage(*imgView->GetLegend()));
}

void rsMainWindow::readCsvFile(const char *fname)
{
	imgView->beginWait();

	loadSelectors();
	rdr.ReadParticles(fname, particles);
	updateParticleLimits();
	loadLimits();

	double maxXm = rdr.GetMaxXm();
	double maxYm = rdr.GetMaxYm();
	int w = img_width;
	int h = img_height;
	if (maxXm > 0.001 && maxYm > 0.001) {
		double scx = (img_width - 20.) / maxXm;
		double scy = (img_height - 20.) / maxYm;
		scale_to_img = (scx < scy) ? scx : scy;
		if (scale_to_img > 1.) scale_to_img = 1.;
		// std::cout << "scale_to_img = " << scale_to_img << std::endl;
		w = int(scale_to_img * maxXm + 25.); w -= w % 10;
		h = int(scale_to_img * maxYm + 25.); h -= h % 10;
	}
	else
		scale_to_img = 1.;
	double area = 0.;
	double maxArea = 0.;
	for (size_t i = 0; i < particles.size(); i++) {
		rsParticle &pt = particles[i];
		pt.p.xStart = int(pt.p.xm * scale_to_img + 0.5);
		pt.p.yStart = int(pt.p.ym * scale_to_img + 0.5);
		area += pt.p.Area;
		if (maxArea < pt.p.Area) maxArea = pt.p.Area;
		if (pt.p.xStart < 2) pt.p.xStart = 2;
		else if (pt.p.xStart >= w - 5) pt.p.xStart = w - 5;
		if (pt.p.yStart < 2) pt.p.yStart = 2;
		else if (pt.p.yStart >= h - 5) pt.p.yStart = h - 5;
	}
	max_rad = sqrt(maxArea / PI);
	if (particles.size() > 1) {
		area /= particles.size();
	}
	else area = 25.;
	double side = sqrt(area) * scale_to_img;
	//std::cout << "side = " << side << std::endl;
	double opacity = side*0.75;
	if (opacity > 1.) opacity = 1.;
	else if (opacity < 0.1) opacity = 0.1;
	// std::cout << "opacity = " << opacity << std::endl;

	int idx = closest_cell_size_index(side * 0.6);
	int vsz = cell_sizes[idx].vsz;

	_busy = true;
	cbCellSize->setCurrentIndex(idx);
	slOpacity->setValue(int(opacity * 100.));
	opacity = slOpacity->value() * 0.01;

	// int vsz = int(side*0.5);
	// if (vsz > 5) vsz = 5;
	// vsz = 5;

	imgView->InitializeView();
	imgView->SetParticles(&particles, w, h, opacity, vsz);

	setSelectors();
	circles_ok = true;
	resetAct->setEnabled(true);

	imgView->ResetView(true);
	imgView->ImageDataModified();
	legendLabel->setPixmap(QPixmap::fromImage(*imgView->GetLegend()));

	imgView->endWait();
	_busy = false;
}

void rsMainWindow::writeCsvFile(const char *fname)
{
	std::ofstream fo(fname, std::ios::out | std::ios::binary);
	if (fo.is_open()) {
		GFigure & inf = imgView->GetInner()->toGFigure(scale_to_img);
		GFigure & outf = imgView->GetOuter()->toGFigure(scale_to_img);
		fo.write(rdr.GetDataPointer(0), rdr.GetHeadersLen());
		for (rsParticle &pt : particles) {
			pt.p.selected = 0.;
			GPoint p(pt.p.xm, pt.p.ym);
			if (!outf.IsInside(p)) continue;
			if (inf.IsInside(p)) continue;

			pt.p.selected = 1.;
			size_t sz = pt.endpos - pt.startpos;
			fo.write(rdr.GetDataPointer(pt.startpos), sz);
		}
		fo.close();
	}

	// Also write accompanying .sel file (JSON format)
	loadSelectors();
	std::string txtfn(fname);
	txtfn.resize(txtfn.size()-4);
	txtfn = txtfn + ".sel";

	QJsonObject jobj;
	jobj["inner"] = imgView->GetInner()->toGFigure(scale_to_img).toJson();
	jobj["outer"] = imgView->GetOuter()->toGFigure(scale_to_img).toJson();
	QJsonDocument json = QJsonDocument(jobj);

	// cout << json.toJson(QJsonDocument::Indented).toStdString().c_str() << std::endl;
	QFile qfo(txtfn.c_str());
	if (qfo.open(QIODevice::WriteOnly)) {
		qfo.write(json.toJson(QJsonDocument::Indented));
		qfo.close();
	}

	int cparIdx = cbParam->findText(tr("In Selection"));
	if (cparIdx == cbParam->currentIndex()) {
		visualParamChanged(cparIdx);
	}
}

void rsMainWindow::setSelectors()
{
	int w = imgView->GetWidth();
	int h = imgView->GetHeight();
	innerCir.fromGFigure(inGCir, scale_to_img);
	if (!innerCir.size_ok(w, h)) {
		// std::cout << "Inner cir: R=" << innerCir.GetR() << " XC=" << innerCir.GetXC() << " YC=" << innerCir.GetYC() << " w=" << w << " h=" << h << std::endl;
		resetInner(innerCir);
	}
	innerPoly.fromGFigure(inGPoly, scale_to_img);
	if (!innerPoly.size_ok(w, h)) resetInner(innerPoly);

	outerCir.fromGFigure(outGCir, scale_to_img);
	if (!outerCir.size_ok(w, h)) resetOuter(outerCir);
	outerPoly.fromGFigure(outGPoly, scale_to_img);
	if (!outerPoly.size_ok(w, h)) resetOuter(outerPoly);

	innerChanged(cbInner->currentIndex());
	outerChanged(cbOuter->currentIndex());
}

void rsMainWindow::innerChanged(int idx)
{
	switch (idx) {
	case 0:
		imgView->SetInner(&innerCir);
		break;
	case 1:
		imgView->SetInner(&innerPoly);
		break;
	}
	imgView->repaint();
}
void rsMainWindow::outerChanged(int idx)
{
	switch (idx) {
	case 0:
		imgView->SetOuter(&outerCir);
		break;
	case 1:
		imgView->SetOuter(&outerPoly);
		break;
	}
	imgView->repaint();
}

void rsMainWindow::resetSelectors()
{
	int w = imgView->GetWidth();
	int h = imgView->GetHeight();
	int sz = h; if (sz > w) sz = w;
	int inr0 = sz * 6 / 20;
	int outr0 = sz * 8 / 20;
	innerCir.moveto(w/2, h/2+20, inr0);
	innerPoly.moveto(w / 2, h / 2 + 20, inr0);
	outerCir.moveto(w/2, h/2-20, outr0);
	outerPoly.moveto(w / 2, h / 2 - 20, outr0);
	innerChanged(cbInner->currentIndex());
	outerChanged(cbOuter->currentIndex());
	imgView->ResetView(true);
}

void rsMainWindow::resetInner(rsFigure &inner)
{
	int w = imgView->GetWidth();
	int h = imgView->GetHeight();
	int sz = h; if (sz > w) sz = w;
	int inr0 = sz * 6 / 20;
	inner.moveto(w / 2, h / 2 + 20, inr0);
}
void rsMainWindow::resetOuter(rsFigure &outer)
{
	int w = imgView->GetWidth();
	int h = imgView->GetHeight();
	int sz = h; if (sz > w) sz = w;
	int outr0 = sz * 8 / 20;
	outer.moveto(w / 2, h / 2 - 20, outr0);
}

void rsMainWindow::arcButtonClicked()
{
	if (!circles_ok) return;
	GFigure & outf = imgView->GetOuter()->toGFigure(scale_to_img);
	GRect orect = outf.GetRect(max_rad);
	
	double sel_area = outf.area();
	int npt = 0;
	double ptarea = 0.;
	int nbpt = 0;
	double bptarea = 0.;
	for (rsParticle &pt : particles) {
		GCell c(pt.p.xm, pt.p.ym, pt.p.Area);
		if (!orect.IsInside(c)) {
			pt.p.selected = 0.;
			continue;
			}
		if (outf.IsBorder(c)) {
			++nbpt;
			bptarea += pt.p.Area;
			pt.p.selected = 0.5;
			continue;
		}
		if (!outf.IsInside(c)) {
			pt.p.selected = 0.;
			continue;
		}
		++npt;
		ptarea += pt.p.Area;
		pt.p.selected = 1.;
	}

	double carea = ptarea + bptarea / 2.;
	double perc = carea * 100. / sel_area;

	QString text = QString::asprintf(
		"Outer Selection Area: %2.2lf\n"
		"Cells fully inside: %d\n"
		"Inside Cell Area: %2.2lf\n"
		"Cells on border: %d\n"
		"Border Cell Area: %2.2lf\n"
		"Combined Cell Area: %2.2lf\n"
		"% Cell Area: %2.4lf\n"
		,
		sel_area,
		npt,
		ptarea,
		nbpt,
		bptarea,
		carea,
		perc);
	arcText->setText(text);

	int cparIdx = cbParam->findText(tr("In Selection"));
	if (cparIdx == cbParam->currentIndex()) {
		visualParamChanged(cparIdx);
	}
}

void rsMainWindow::loadSelectors()
{
	if (!circles_ok) return;
	inGCir = innerCir.toGCircle(scale_to_img);
	outGCir = outerCir.toGCircle(scale_to_img);
	inGPoly = innerPoly.toGPolygon(scale_to_img);
	outGPoly = outerPoly.toGPolygon(scale_to_img);
}

void rsMainWindow::loadState()
{
	QFile fi(stateFile.filePath());

	if (fi.open(QIODevice::ReadOnly)) {
		QJsonDocument json = QJsonDocument::fromJson(fi.readAll());
		fi.close();

		QJsonObject jobj = json.object();
		// cout << json.toJson(QJsonDocument::Indented).toStdString().c_str() << std::endl;
		if (jobj["srcDir"].isString())
			srcDir.setPath(jobj["srcDir"].toString());
		if (jobj["srcDialogState"].isString())
			srcDialogState = QByteArray::fromBase64(QByteArray(jobj["srcDialogState"].toString().toStdString().c_str()));
		if (jobj["tgtDir"].isString())
			tgtDir.setPath(jobj["tgtDir"].toString());
		if (jobj["tgtDialogState"].isString())
			tgtDialogState = QByteArray::fromBase64(QByteArray(jobj["tgtDialogState"].toString().toStdString().c_str()));

		if (jobj["innerCir"].isObject())
			inGCir.fromJson(jobj["innerCir"].toObject());
		if (jobj["outerCir"].isObject())
			outGCir.fromJson(jobj["outerCir"].toObject());
		if (jobj["innerPoly"].isObject())
			inGPoly.fromJson(jobj["innerPoly"].toObject());
		if (jobj["outerPoly"].isObject())
			outGPoly.fromJson(jobj["outerPoly"].toObject());
		if (jobj["inner"].isDouble())
			cbInner->setCurrentIndex(jobj["inner"].toInt());
		if (jobj["outer"].isDouble())
			cbOuter->setCurrentIndex(jobj["outer"].toInt());

		// std::cout << "outX=" << outX << " outY=" << outY << " outR=" << outR << std::endl;
	}
}

void rsMainWindow::saveState()
{
	QJsonObject jobj;
	jobj["srcDir"] = srcDir.path();
	jobj["srcDialogState"] = QString(srcDialogState.toBase64());
	jobj["tgtDir"] = tgtDir.path();
	jobj["tgtDialogState"] = QString(tgtDialogState.toBase64());

	jobj["innerCir"] = innerCir.toGCircle(scale_to_img).toJson();
	jobj["outerCir"] = outerCir.toGCircle(scale_to_img).toJson();
	jobj["innerPoly"] = innerPoly.toGPolygon(scale_to_img).toJson();
	jobj["outerPoly"] = outerPoly.toGPolygon(scale_to_img).toJson();
	jobj["inner"] = QJsonValue(cbInner->currentIndex());
	jobj["outer"] = QJsonValue(cbOuter->currentIndex());

	QJsonDocument json = QJsonDocument(jobj);

	// cout << json.toJson(QJsonDocument::Indented).toStdString().c_str() << std::endl;
	QFile fo(stateFile.filePath());
	if (fo.open(QIODevice::WriteOnly)) {
		fo.write(json.toJson(QJsonDocument::Indented));
		fo.close();
	}
}
