#ifndef _abstractTool_H
#define _abstractTool_H

/***************************************************************
 * abstractTool.h
 * @Author:      Jonathan Verner (jonathan.verner@matfyz.cz)
 * @License:     GPL v2.0 or later
 * @Created:     2008-10-17.
 * @Last Change: 2008-10-17.
 * @Revision:    0.0
 * Description:
 * Usage:
 * TODO:
 *CHANGES:
 ***************************************************************/
#include <QtCore/QString>
#include <QtCore/QDate>
#include <QtCore/QTime>

#include <QtGui/QPixmap>
#include <QtGui/QGraphicsItem>

class QToolBar;
class QStackedWidget;
class abstractAnnotation;
class QWidget;
class QPoint;
class pdfScene;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneHoverEvent;
class toolBox;
class viewEvent;
class QLabel;

namespace PoDoFo { 
  class PdfAnnotation;
  class PdfDocument;
}

class pdfCoords;

class abstractTool : public QObject { 
	private:
		QString toolName;

	protected:
		QStackedWidget *editArea;
		toolBox *toolBar;
		pdfScene *scene;
		QWidget *editor;
		abstractAnnotation *currentEditItem;
		QString author;


		void setToolName( QString ToolName ) { toolName = ToolName; };

	public:
		 abstractTool( pdfScene *Scene, toolBox *ToolBar, QStackedWidget *EditArea  ): editArea(EditArea), scene(Scene), toolBar(ToolBar), currentEditItem( NULL ) {} ;

		 QString getAuthor() const { return author; };
		 void setAuthor( QString Author ) { author = Author; };

		 QString getToolName() { return toolName; };

		 /* Returns a pointer to a newly created annotation, if the annotation was recognized,
		  * otherwise returns NULL */
		 virtual abstractAnnotation *processAnnotation( PoDoFo::PdfAnnotation *annotation, pdfCoords *transform ) = 0;

		 /* Called when the user wants to add a new annotation at scenePos 
		  * (scene coordinates) */
		 virtual void newActionEvent( const QPointF *scenePos ) = 0;


		 /* Called by an item, which wants to be edited. The item passes
		  * a reference to itself */
		 virtual void editItem( abstractAnnotation *item );

		 /* Called by the view when an event happens. Returns true, if
		  * tool handles the event, otherwise returns false; */
		 virtual bool handleEvent( viewEvent *ev );

		 /* Returns true, if the tool handles events for the item it */
		 virtual bool acceptEventsFor( QGraphicsItem *it )=0;

		 friend class abstractAnnotation;

};

class abstractAnnotation : public QGraphicsItem { 
	private:
		QPixmap toolTipPixMap, icon;
		QString toolTipRichText;
		bool waitingForToolTip, haveToolTip, moved, showingToolTip;
		enum toolTipType { pixmap, text };
		toolTipType tp;

		QString author;
		QDate date;
		QTime time;


		abstractTool *myTool;


	protected:
		bool movable;
		void setMyToolTip(const QPixmap &pixMap);
		void setMyToolTip(const QString &richText);
		void setIcon( const QPixmap &icon);


	public:
		static bool isA( PoDoFo::PdfAnnotation *annotation ) { return false; };
 
	public:
		abstractAnnotation( abstractTool *tool );


		bool showToolTip( const QPoint &scPos );
		void hideToolTip();
		bool editSelf();
		bool hasToolTip();

		bool isMovable() { return movable; };
		void setAuthor( QString a ) { author = a; };
		void setDate( QDate d ) { date = d; };
		void setTime( QTime t ) { time = t; };
		QString getAuthor() { return author; };
		QDate getDate() { return date; };
		QTime getTime() { return time; };

		virtual QRectF boundingRect() const;
		virtual void paint( QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget );



		virtual void saveToPdfPage( PoDoFo::PdfDocument *document, int page ) = 0;

};





#endif /* _abstractTool_H */
