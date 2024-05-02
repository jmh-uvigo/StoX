/********************************************************************************************
 *                                                                                          *
 *  StoX                                                                                    *
 *  ----                                                                                    *
 *                                                                                          *
 *  Copyright 2008-2024 J.Martín-Herrero (Universiy of Vigo, Spain).                        *
 *                        julio@uvigo.es                                                    *
 *                                                                                          *
 *  This file is part of StoX.                                                              *
 *                                                                                          *
 *  StoX is free software: you can redistribute it and/or modify it under the terms of      *
 *  the GNU General Public License as published by the Free Software Foundation, either     *
 *  version 3 of the License, or any later version.                                         *
 *                                                                                          *
 *  StoX is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;       *
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR        *
 *  PURPOSE. See the GNU General Public License for more details.                           *
 *                                                                                          *
 *  You should have received a copy of the GNU General Public License along with StoX.      *
 *  If not, see <https://www.gnu.org/licenses/>.                                            *
 *                                                                                          *
 *                                                                                          *
 ********************************************************************************************/

#include "stox.h"

#include <QApplication>
#include <QStyleFactory>
#include <QSplashScreen>
#include <thread>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setStyle(QStyleFactory::create("windows"));

    QPixmap pixmap(":/images/Resources/Corema.png");
    QSplashScreen splash(pixmap,Qt::WindowStaysOnTopHint);
    splash.show();
    a.processEvents();


    Stox w;
    w.show();

    splash.finish(&w);

    return a.exec();
}


