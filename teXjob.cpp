/***************************************************************
 * teXjob.cpp
 * @Author:      Jonathan Verner (jonathan.verner@matfyz.cz)
 * @License:     GPL v2.0 or later
 * @Created:     2008-11-17.
 * @Last Change: 2008-11-17.
 * @Revision:    0.0
 * Description:
 * Usage:
 * TODO:
 *CHANGES:
 ***************************************************************/

#include <QtCore/QDebug>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QRegExp>

#include <poppler-qt4.h>

#include "teXjob.h"


QString compileJob::latexPath("/usr/bin/pdflatex");
QString compileJob::gsPath("/usr/bin/gs");
bool compileJob::paths_ok = true;

compileJob::compileJob():
	proc(NULL), jobStarted(false)
{
  proc = new QProcess( this );
  proc->setWorkingDirectory( QDir::tempPath() );
  tmpSRC.setFileTemplate( QDir::tempPath()+"/testTeXRenderXXXXXX" );
}

compileJob::~compileJob() { 
  if ( jobStarted || (proc->state() != QProcess::NotRunning)) {
    proc->kill();
  }
  delete proc;
}

void compileJob::setPaths( QString latex, QString gs ) { 
  if ( checkPaths( latex, gs ) ) { 
    latexPath = latex;
    gsPath = gs;
    paths_ok = true;
  }
}

bool compileJob::pathsOK() { 
  return paths_ok;
}

bool compileJob::checkPaths( QString latex, QString gs ) { 
  return true;
}

QString compileJob::texName2Pdf( QString fname ) { 
  return fname+".pdf";
}

void compileJob::removeTempFiles() { 
  QFileInfo info( tmpSRC );
  QString baseName=info.baseName();
  QDir dir = info.absoluteDir();
  dir.setNameFilters( QStringList(baseName +".*") );
  foreach( QString fl, dir.entryList() ) {
    if ( ! fl.endsWith( ".pdf" ) ) dir.remove( fl );
  }
  tmpSRC.remove();
}

QRectF compileJob::parseGSOutput() { 
  Q_ASSERT( proc );
  QRegExp exp("HiResBoundingBox: *([0-9.]*) *([0-9.]*) *([0-9.]*) *([0-9.]*) *");
  QString output( proc->readAllStandardError() );
  if ( exp.indexIn( output ) > -1 ) {
    qreal x = exp.cap(1).toFloat(), y = exp.cap(2).toFloat();
    qreal w = (exp.cap(3).toFloat()-x), h = (exp.cap(4).toFloat()-y);
    return QRectF( x, y, w, h );
  } else 
    return QRectF( 0,0,0,0 );
}






void compileJob::start( QString latexSource ) { 
  if ( jobStarted ) { 
    qWarning() << "Cannot start while another job in progress. Please use the restart method";
    return;
  }
  if ( ! tmpSRC.open() ) {
    qWarning() << "Cannot open temporary file.";
    emit finished( QString(""), QRectF(0,0,0,0), false );
  }
  tmpSRC.write(latexSource.toLocal8Bit()); //FIXME: can fail if unexpected characters
  tmpSRC.flush();
  jobStarted=true;
  disconnect( proc, 0, 0, 0 );
  connect( proc, SIGNAL( finished(int,QProcess::ExitStatus) ), this, SLOT(texJobFinished(int,QProcess::ExitStatus)) ); 
  proc->start( latexPath , ( QStringList() << "-interaction=nonstopmode" << tmpSRC.fileName() ) );
}

void compileJob::restart( QString latexSource ) { 
  disconnect( proc, 0, 0, 0 );
  if ( jobStarted ||  (proc->state() != QProcess::NotRunning) ) { 
    proc->kill();
    removeTempFiles();
    jobStarted=false;
  }
  start( latexSource );
}

void compileJob::kill() { 
   disconnect( proc, 0, 0, 0 );
   if ( (proc->state() != QProcess::NotRunning) ) { 
     proc->kill();
     removeTempFiles();
   }
   if ( jobStarted ) { 
     jobStarted=false;
     emit finished( QString(""),QRectF(0,0,0,0), false );
   }
}

bool compileJob::running() { 
  return jobStarted;
}

void compileJob::texJobFinished( int eCode, QProcess::ExitStatus eStat ) {
  pdfFName = texName2Pdf(tmpSRC.fileName());
  disconnect( proc, 0, 0, 0 );
  connect( proc, SIGNAL( finished(int,QProcess::ExitStatus) ), this, SLOT(gsJobFinished(int,QProcess::ExitStatus)) ); 
  proc->start( gsPath, (QStringList() << "-sDEVICE=bbox" <<"-dBATCH"<<"-dNOPAUSE"<<"-f"<<pdfFName) );
}

void compileJob::gsJobFinished( int eCode, QProcess::ExitStatus eStat ) {
  removeTempFiles();
  disconnect( proc, 0, 0, 0 );
  jobStarted=false;
  QRectF bBox = parseGSOutput();
  emit finished( pdfFName, bBox, true );
}


renderItem::~renderItem() { 
  if ( pdf ) delete pdf; // FIXME: also delete the underlying file
  delete localLoop;
}

renderItem::renderItem( QString source, QString preamb ):
	src(source), pre(preamb), ready(false), bBox(0,0,0,0), job_id(-1), pdf(NULL),
	waiting_for_job(false)
{ 
  connect( &job, SIGNAL( finished(QString,QRectF,bool) ), this, SLOT( pdfReady(QString,QRectF,bool) ) );
  localLoop = new QEventLoop( this );
}

void renderItem::pdfReady( QString pdfFName, QRectF BBox, bool status ) { 
  if ( ! status ) {
    ready = false;
    qWarning() << "renderItem::pdfReady: Error compiling latex. Job failed.";
  } else { 
    if ( pdf ) delete pdf;
    pdf = Poppler::Document::load( pdfFName );
    pdf->setRenderHint( Poppler::Document::TextAntialiasing, true );
    pdf->setRenderHint( Poppler::Document::Antialiasing, true );
    bBox=BBox;
    ready = true;
  }
  if ( waiting_for_job ) {
    waiting_for_job = false;
    localLoop->exit();
  }
  if ( status && job_id > -1 ) {
    emit renderingReady( job_id );
    job_id = -1;
  }
}

void renderItem::wait_for_job() { 
  if ( waiting_for_job ) {
    qDebug() << "Warning, already waiting for job " << job_id;
  } else { 
    waiting_for_job = true;
    localLoop->exec();
  }
}

QString renderItem::getLaTeX( QString Src, QString Pre ) {
  return "\\documentclass[10pt,a4paper]{article}\n\\usepackage{amssymb}\n"+Pre+"\\begin{document}\n\\pagestyle{empty}\n\\begin{minipage}{5cm}\n"+Src+"\n\\end{minipage}\n\\end{document}\n";
}



void renderItem::preRender( int jobID ) { 
  if ( job.running() ) { 
    if ( job_id == jobID ) return; // Probably already running, no need to run again.
    job.kill();
  };
  job_id = jobID;
  job.start( getLaTeX( src, pre ) );
}

 

void renderItem::updateItem( QString source, QString preambule, int jobID ) { 
  src = source;
  pre = preambule;
  if ( pdf ) delete pdf; //FIXME: also delete the underlying file
  pdf = NULL;
  bBox = QRectF(0,0,0,0);
  ready = false;
  preRender( jobID );
}

int renderItem::size() { 
  return (int) (bBox.width()*bBox.height());
}

QPixmap renderItem::render( qreal zoom, bool format_inline ) { 
  if ( job.running() ) wait_for_job();
  if ( ! ready ) { 
    job.start( getLaTeX( src, pre ) );
    wait_for_job();
  }
  if ( ready && pdf ) { 
    Poppler::Page *pg = pdf->page(0);
    qreal pgHeight = pg->pageSizeF().height();
    qDebug() << "Rendering source ("<<72*zoom<<","<< 72*zoom<<","<< (int) bBox.x()<<","<<(int) (pg->pageSizeF().height()-bBox.y())<<","<<(int) bBox.width()<<","<<(int) bBox.height()<<")"; 
    QImage image = pg->renderToImage( 72*zoom, 72*zoom/*, (int) bBox.x(),(int) (pg->pageSizeF().height()-bBox.y()),(int) bBox.width(),(int) bBox.height()*/ );
    delete pg;
    QPixmap px = QPixmap::fromImage( image );
    qDebug() << "Rendered image: "<< image.width() << " x "<< image.height();
    return px.copy( bBox.x()*zoom, (pgHeight-bBox.y()-bBox.height())*zoom, bBox.width()*zoom, bBox.height()*zoom);
  } else { 
    return QPixmap();
  }
}



#include "teXjob.moc"
