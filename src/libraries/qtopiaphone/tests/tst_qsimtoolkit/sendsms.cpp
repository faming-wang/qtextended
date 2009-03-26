/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2009 Trolltech ASA.
**
** Contact: Qt Extended Information (info@qtextended.org)
**
** This file may be used under the terms of the GNU General Public License
** version 2.0 as published by the Free Software Foundation and appearing
** in the file LICENSE.GPL included in the packaging of this file.
**
** Please review the following information to ensure GNU General Public
** Licensing requirements will be met:
**     http://www.fsf.org/licensing/licenses/info/GPLv2.html.
**
**
****************************************************************************/

#include "tst_qsimtoolkit.h"
#include <qsmsmessage.h>

#ifndef SYSTEMTEST

// Test encoding and decoding of EVENT DOWNLOAD envelopes based on the
// Test encoding and decoding of SEND SHORT MESSAGE commands based on the
// GCF test strings in GSM 51.010, section 27.22.4.10 - SEND SHORT MESSAGE.
void tst_QSimToolkit::testEncodeSendSMS_data()
{
    QSimToolkitData::populateDataSendSMS();
}
void tst_QSimToolkit::testEncodeSendSMS()
{
    QFETCH( QByteArray, data );
    QFETCH( QByteArray, resp );
    QFETCH( QByteArray, tpdu );
    QFETCH( int, resptype );
    QFETCH( QString, text );
    QFETCH( QString, number );
    QFETCH( bool, smsPacking );
    QFETCH( int, iconId );
    QFETCH( bool, iconSelfExplanatory );
    QFETCH( QByteArray, textAttribute );
    QFETCH( QString, html );
    QFETCH( int, options );

    // Output a dummy line to give some indication of which test we are currently running.
    qDebug() << "";

    // Check that the command PDU can be parsed correctly.
    QSimCommand decoded = QSimCommand::fromPdu(data);
    QVERIFY( decoded.type() == QSimCommand::SendSMS );
    QVERIFY( decoded.destinationDevice() == QSimCommand::Network );
    QCOMPARE( decoded.text(), text );
    if ( text.isEmpty() ) {
        if ( ( options & QSimCommand::EncodeEmptyStrings ) != 0 )
            QVERIFY( decoded.suppressUserFeedback() );
        else
            QVERIFY( !decoded.suppressUserFeedback() );
    } else {
        QVERIFY( !decoded.suppressUserFeedback() );
    }
    QCOMPARE( decoded.number(), number );
    QCOMPARE( decoded.smsPacking(), smsPacking );
    QCOMPARE( (int)decoded.iconId(), iconId );
    QCOMPARE( decoded.iconSelfExplanatory(), iconSelfExplanatory );
    QCOMPARE( decoded.textAttribute(), textAttribute );
    if ( !textAttribute.isEmpty() )
        QCOMPARE( decoded.textHtml(), html );

    // Check the final TPDU.  If packing is specified, we have to parse the SMS
    // and then re-encode it with the 7-bit alphabet.
    QByteArray newtpdu = decoded.extensionField(0x8B);
    if ( smsPacking ) {
        // Add dummy service center address that isn't in the TPDU before decoding.
        QSMSMessage msg = QSMSMessage::fromPdu( QByteArray( 1, 0 ) + newtpdu );
        msg.setDataCodingScheme( msg.dataCodingScheme() & 0xF3 );   // Convert to 7-bit

        // Convert back into a pdu and strip off the dummy service center address.
        newtpdu = msg.toPdu().mid(1);
    }

    // The TPDU in the command will have a message reference of 0.
    // We need to change it to the transmission message reference of 1
    // before we do the comparison.
    newtpdu[1] = (char)1;
    QCOMPARE( newtpdu, tpdu );

    // Check that the original command PDU can be reconstructed correctly.
    QByteArray encoded = decoded.toPdu( (QSimCommand::ToPduOptions)options );
    QCOMPARE( encoded, data );

    // Check that the terminal response PDU can be parsed correctly.
    QSimTerminalResponse decodedResp = QSimTerminalResponse::fromPdu(resp);
    QVERIFY( data.contains( decodedResp.commandPdu() ) );
    if ( resptype < 0x0100 ) {
        QVERIFY( decodedResp.result() == (QSimTerminalResponse::Result)resptype );
        QVERIFY( decodedResp.causeData().isEmpty() );
        QVERIFY( decodedResp.cause() == QSimTerminalResponse::NoSpecificCause );
    } else {
        QVERIFY( decodedResp.result() == (QSimTerminalResponse::Result)(resptype >> 8) );
        QVERIFY( decodedResp.causeData().size() == 1 );
        QVERIFY( decodedResp.cause() == (QSimTerminalResponse::Cause)(resptype & 0xFF) );
    }

    // Check that the original terminal response PDU can be reconstructed correctly.
    QCOMPARE( decodedResp.toPdu(), resp );
}

// Test that SEND SMS commands can be properly delivered to a client
// application and that the client application can respond appropriately.
void tst_QSimToolkit::testDeliverSendSMS_data()
{
    QSimToolkitData::populateDataSendSMS();
}
void tst_QSimToolkit::testDeliverSendSMS()
{
    QFETCH( QByteArray, data );
    QFETCH( QByteArray, resp );
    QFETCH( QByteArray, tpdu );
    QFETCH( int, resptype );
    QFETCH( QString, text );
    QFETCH( QString, number );
    QFETCH( bool, smsPacking );
    QFETCH( int, iconId );
    QFETCH( bool, iconSelfExplanatory );
    QFETCH( QByteArray, textAttribute );
    QFETCH( QString, html );
    QFETCH( int, options );

    Q_UNUSED(html);

    // Output a dummy line to give some indication of which test we are currently running.
    qDebug() << "";

    // Clear the client/server state.
    server->clear();
    deliveredCommand = QSimCommand();

    // Compose and send the command.
    QSimCommand cmd;
    cmd.setType( QSimCommand::SendSMS );
    cmd.setDestinationDevice( QSimCommand::Network );
    cmd.setText( text );
    if ( text.isEmpty() && ( options & QSimCommand::EncodeEmptyStrings ) != 0 )
        cmd.setSuppressUserFeedback( true );
    cmd.setNumber( number );
    cmd.setSmsPacking( smsPacking );
    cmd.setIconId( (uint)iconId );
    cmd.setIconSelfExplanatory( iconSelfExplanatory );
    cmd.setTextAttribute( textAttribute );
    QByteArray newtpdu = tpdu;
    if ( smsPacking ) {
        // To prevent truncation of the 1.4.1 test data during 7-bit to 8-bit conversion,
        // we extract the TPDU from "data" rather than use "tpdu".
        newtpdu = QSimCommand::fromPdu( data ).extensionField(0x8B);
    }
    newtpdu[1] = (char)0;
    cmd.addExtensionField( 0x8B, newtpdu );
    server->emitCommand( cmd );

    // Wait for the command to arrive in the client.
    QVERIFY( QFutureSignal::wait( this, SIGNAL(commandSeen()), 100 ) );

    // Verify that the command was delivered exactly as we asked.
    QVERIFY( deliveredCommand.type() == cmd.type() );
    QVERIFY( deliveredCommand.text() == cmd.text() );
    QVERIFY( deliveredCommand.suppressUserFeedback() == cmd.suppressUserFeedback() );
    QVERIFY( deliveredCommand.number() == cmd.number() );
    QVERIFY( deliveredCommand.smsPacking() == cmd.smsPacking() );
    QVERIFY( deliveredCommand.iconId() == cmd.iconId() );
    QVERIFY( deliveredCommand.iconSelfExplanatory() == cmd.iconSelfExplanatory() );
    QVERIFY( deliveredCommand.extensionData() == cmd.extensionData() );
    QVERIFY( deliveredCommand.textAttribute() == cmd.textAttribute() );
    QCOMPARE( deliveredCommand.toPdu( (QSimCommand::ToPduOptions)options ), data );

    // The terminal response should have been sent immediately to ack reception of the command.
    QCOMPARE( server->responseCount(), 1 );
    QCOMPARE( server->envelopeCount(), 0 );
    if ( resptype != 0x0004 ) {
        QCOMPARE( server->lastResponse(), resp );
    } else {
        // We cannot test the "icon not displayed" case because the qtopiaphone
        // library will always respond with "command performed successfully".
        // Presumably the Qtopia user interface can always display icons.
        QByteArray resp2 = resp;
        resp2[resp2.size() - 1] = 0x00;
        QCOMPARE( server->lastResponse(), resp2 );
    }
}

// Test the user interface in "simapp" for SEND SHORT MESSAGE.
void tst_QSimToolkit::testUISendSMS_data()
{
    QSimToolkitData::populateDataSendSMS();
}
void tst_QSimToolkit::testUISendSMS()
{
    QFETCH( QByteArray, data );
    QFETCH( QByteArray, resp );
    QFETCH( QByteArray, tpdu );
    QFETCH( int, resptype );
    QFETCH( QString, text );
    QFETCH( QString, number );
    QFETCH( bool, smsPacking );
    QFETCH( int, iconId );
    QFETCH( bool, iconSelfExplanatory );
    QFETCH( QByteArray, textAttribute );
    QFETCH( QString, html );
    QFETCH( int, options );

    Q_UNUSED(html);
    Q_UNUSED(options);

    // Skip tests that we cannot test using the "simapp" UI.
    if ( resptype == 0x0004 ) {     // Icon not displayed
        QSKIP( "", SkipSingle );
    }

    // Output a dummy line to give some indication of which test we are currently running.
    qDebug() << "";

    // Create the command to be tested.
    QSimCommand cmd;
    cmd.setType( QSimCommand::SendSMS );
    cmd.setDestinationDevice( QSimCommand::Network );
    cmd.setText( text );
    if ( text.isEmpty() && ( options & QSimCommand::EncodeEmptyStrings ) != 0 )
        cmd.setSuppressUserFeedback( true );
    cmd.setNumber( number );
    cmd.setSmsPacking( smsPacking );
    cmd.setIconId( (uint)iconId );
    cmd.setIconSelfExplanatory( iconSelfExplanatory );
    cmd.setTextAttribute( textAttribute );
    QByteArray newtpdu = tpdu;
    if ( smsPacking ) {
        // To prevent truncation of the 1.4.1 test data during 7-bit to 8-bit conversion,
        // we extract the TPDU from "data" rather than use "tpdu".
        newtpdu = QSimCommand::fromPdu( data ).extensionField(0x8B);
    }
    newtpdu[1] = (char)0;
    cmd.addExtensionField( 0x8B, newtpdu );

    // Set up the server with the command, ready to be selected
    // from the "Run Test" menu item on the test menu.
    server->startUsingTestMenu( cmd );
    QVERIFY( waitForView( SimMenu::staticMetaObject ) );

    // Clear the server state just before we request the actual command under test.
    server->clear();

    // Select the first menu item.
    select();

    // Wait for the text to display.  If user feedback is suppressed, then
    // the command is supposed to be performed silently.
    if ( cmd.suppressUserFeedback() )
        QVERIFY( !waitForView( SimText::staticMetaObject ) );
    else
        QVERIFY( waitForView( SimText::staticMetaObject ) );

    // Check that the response is what we expected.  The response is
    // sent automatically by the back-end and is always "success".
    QCOMPARE( server->responseCount(), 1 );
    QCOMPARE( server->envelopeCount(), 0 );
    QCOMPARE( server->lastResponse(), resp );
}

#endif // !SYSTEMTEST

// Populate data-driven tests for SEND SHORT MESSAGE from the GCF test cases
// in GSM 51.010, section 27.22.4.10.
void QSimToolkitData::populateDataSendSMS()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<QByteArray>("resp");
    QTest::addColumn<QByteArray>("tpdu");
    QTest::addColumn<int>("resptype");
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("number");
    QTest::addColumn<bool>("smsPacking");
    QTest::addColumn<int>("iconId");
    QTest::addColumn<bool>("iconSelfExplanatory");
    QTest::addColumn<QByteArray>("textAttribute");
    QTest::addColumn<QString>("html");
    QTest::addColumn<int>("options");

    static unsigned char const data_1_1_1[] =
        {0xD0, 0x37, 0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x81, 0x83, 0x85,
         0x07, 0x53, 0x65, 0x6E, 0x64, 0x20, 0x53, 0x4D, 0x86, 0x09, 0x91, 0x11,
         0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0xF8, 0x8B, 0x18, 0x01, 0x00, 0x09,
         0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C, 0x54, 0x65, 0x73,
         0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65};
    static unsigned char const resp_1_1_1[] =
        {0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
    static unsigned char const tpdu_1_1_1[] =
        {0x01, 0x01, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C,
         0x54, 0x65, 0x73, 0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65};
    QTest::newRow( "SEND SHORT MESSAGE 1.1.1 - GCF 27.22.4.10.1" )
        << QByteArray( (char *)data_1_1_1, sizeof(data_1_1_1) )
        << QByteArray( (char *)resp_1_1_1, sizeof(resp_1_1_1) )
        << QByteArray( (char *)tpdu_1_1_1, sizeof(tpdu_1_1_1) )
        << 0x0000       // Command performed successfully
        << QString( "Send SM" )
        << QString( "+112233445566778" )
        << false        // Packing flag
        << 0 << false   // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::NoPduOptions );

    static unsigned char const data_1_2_1[] =
        {0xD0, 0x32, 0x81, 0x03, 0x01, 0x13, 0x01, 0x82, 0x02, 0x81, 0x83, 0x85,
         0x07, 0x53, 0x65, 0x6E, 0x64, 0x20, 0x53, 0x4D, 0x86, 0x09, 0x91, 0x11,
         0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0xF8, 0x8B, 0x13, 0x01, 0x00, 0x09,
         0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x07, 0x53, 0x65, 0x6E,
         0x64, 0x20, 0x53, 0x4D};
    static unsigned char const resp_1_2_1[] =
        {0x81, 0x03, 0x01, 0x13, 0x01, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
    static unsigned char const tpdu_1_2_1[] =
        {0x01, 0x01, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF0, 0x07,
         0xD3, 0xB2, 0x9B, 0x0C, 0x9A, 0x36, 0x01};
    QTest::newRow( "SEND SHORT MESSAGE 1.2.1 - GCF 27.22.4.10.1" )
        << QByteArray( (char *)data_1_2_1, sizeof(data_1_2_1) )
        << QByteArray( (char *)resp_1_2_1, sizeof(resp_1_2_1) )
        << QByteArray( (char *)tpdu_1_2_1, sizeof(tpdu_1_2_1) )
        << 0x0000       // Command performed successfully
        << QString( "Send SM" )
        << QString( "+112233445566778" )
        << true         // Packing flag
        << 0 << false   // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::NoPduOptions );

    static unsigned char const data_1_3_1[] =
        {0xD0, 0x3D, 0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x81, 0x83, 0x85,
         0x0D, 0x53, 0x68, 0x6F, 0x72, 0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61,
         0x67, 0x65, 0x86, 0x09, 0x91, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
         0xF8, 0x8B, 0x18, 0x01, 0x00, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8,
         0x40, 0xF0, 0x0D, 0x53, 0xF4, 0x5B, 0x4E, 0x07, 0x35, 0xCB, 0xF3, 0x79,
         0xF8, 0x5C, 0x06};
    static unsigned char const resp_1_3_1[] =
        {0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
    static unsigned char const tpdu_1_3_1[] =
        {0x01, 0x01, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF0, 0x0D,
         0x53, 0xF4, 0x5B, 0x4E, 0x07, 0x35, 0xCB, 0xF3, 0x79, 0xF8, 0x5C, 0x06};
    QTest::newRow( "SEND SHORT MESSAGE 1.3.1 - GCF 27.22.4.10.1" )
        << QByteArray( (char *)data_1_3_1, sizeof(data_1_3_1) )
        << QByteArray( (char *)resp_1_3_1, sizeof(resp_1_3_1) )
        << QByteArray( (char *)tpdu_1_3_1, sizeof(tpdu_1_3_1) )
        << 0x0000       // Command performed successfully
        << QString( "Short Message" )
        << QString( "+112233445566778" )
        << false        // Packing flag
        << 0 << false   // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::NoPduOptions );

    static unsigned char const data_1_4_1[] =
        {0xD0, 0x81, 0xFD, 0x81, 0x03, 0x01, 0x13, 0x01, 0x82, 0x02, 0x81, 0x83,
         0x85, 0x38, 0x54, 0x68, 0x65, 0x20, 0x61, 0x64, 0x64, 0x72, 0x65, 0x73,
         0x73, 0x20, 0x64, 0x61, 0x74, 0x61, 0x20, 0x6F, 0x62, 0x6A, 0x65, 0x63,
         0x74, 0x20, 0x68, 0x6F, 0x6C, 0x64, 0x73, 0x20, 0x74, 0x68, 0x65, 0x20,
         0x52, 0x50, 0x11, 0x44, 0x65, 0x73, 0x74, 0x69, 0x6E, 0x61, 0x74, 0x69,
         0x6F, 0x6E, 0x11, 0x41, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x86, 0x09,
         0x91, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0xF8, 0x8B, 0x81, 0xAC,
         0x01, 0x00, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0xA0,
         0x54, 0x77, 0x6F, 0x20, 0x74, 0x79, 0x70, 0x65, 0x73, 0x20, 0x61, 0x72,
         0x65, 0x20, 0x64, 0x65, 0x66, 0x69, 0x6E, 0x65, 0x64, 0x3A, 0x20, 0x2D,
         0x20, 0x41, 0x20, 0x73, 0x68, 0x6F, 0x72, 0x74, 0x20, 0x6D, 0x65, 0x73,
         0x73, 0x61, 0x67, 0x65, 0x20, 0x74, 0x6F, 0x20, 0x62, 0x65, 0x20, 0x73,
         0x65, 0x6E, 0x74, 0x20, 0x74, 0x6F, 0x20, 0x74, 0x68, 0x65, 0x20, 0x6E,
         0x65, 0x74, 0x77, 0x6F, 0x72, 0x6B, 0x20, 0x69, 0x6E, 0x20, 0x61, 0x6E,
         0x20, 0x53, 0x4D, 0x53, 0x2D, 0x53, 0x55, 0x42, 0x4D, 0x49, 0x54, 0x20,
         0x6D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x2C, 0x20, 0x6F, 0x72, 0x20,
         0x61, 0x6E, 0x20, 0x53, 0x4D, 0x53, 0x2D, 0x43, 0x4F, 0x4D, 0x4D, 0x41,
         0x4E, 0x44, 0x20, 0x6D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x2C, 0x20,
         0x77, 0x68, 0x65, 0x72, 0x65, 0x20, 0x74, 0x68, 0x65, 0x20, 0x75, 0x73,
         0x65, 0x72, 0x20, 0x64, 0x61, 0x74, 0x61, 0x20, 0x63, 0x61, 0x6E, 0x20,
         0x62, 0x65, 0x20, 0x70, 0x61, 0x73, 0x73, 0x65, 0x64, 0x20, 0x74, 0x72,
         0x61, 0x6E, 0x73, 0x70};
    static unsigned char const resp_1_4_1[] =
        {0x81, 0x03, 0x01, 0x13, 0x01, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
    static unsigned char const tpdu_1_4_1[] =
        {0x01, 0x01, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF0, 0xA0,
         0xD4, 0xFB, 0x1B, 0x44, 0xCF, 0xC3, 0xCB, 0x73, 0x50, 0x58, 0x5E, 0x06,
         0x91, 0xCB, 0xE6, 0xB4, 0xBB, 0x4C, 0xD6, 0x81, 0x5A, 0xA0, 0x20, 0x68,
         0x8E, 0x7E, 0xCB, 0xE9, 0xA0, 0x76, 0x79, 0x3E, 0x0F, 0x9F, 0xCB, 0x20,
         0xFA, 0x1B, 0x24, 0x2E, 0x83, 0xE6, 0x65, 0x37, 0x1D, 0x44, 0x7F, 0x83,
         0xE8, 0xE8, 0x32, 0xC8, 0x5D, 0xA6, 0xDF, 0xDF, 0xF2, 0x35, 0x28, 0xED,
         0x06, 0x85, 0xDD, 0xA0, 0x69, 0x73, 0xDA, 0x9A, 0x56, 0x85, 0xCD, 0x24,
         0x15, 0xD4, 0x2E, 0xCF, 0xE7, 0xE1, 0x73, 0x99, 0x05, 0x7A, 0xCB, 0x41,
         0x61, 0x37, 0x68, 0xDA, 0x9C, 0xB6, 0x86, 0xCF, 0x66, 0x33, 0xE8, 0x24,
         0x82, 0xDA, 0xE5, 0xF9, 0x3C, 0x7C, 0x2E, 0xB3, 0x40, 0x77, 0x74, 0x59,
         0x5E, 0x06, 0xD1, 0xD1, 0x65, 0x50, 0x7D, 0x5E, 0x96, 0x83, 0xC8, 0x61,
         0x7A, 0x18, 0x34, 0x0E, 0xBB, 0x41, 0xE2, 0x32, 0x08, 0x1E, 0x9E, 0xCF,
         0xCB, 0x64, 0x10, 0x5D, 0x1E, 0x76, 0xCF, 0xE1};
    QTest::newRow( "SEND SHORT MESSAGE 1.4.1 - GCF 27.22.4.10.1" )
        << QByteArray( (char *)data_1_4_1, sizeof(data_1_4_1) )
        << QByteArray( (char *)resp_1_4_1, sizeof(resp_1_4_1) )
        << QByteArray( (char *)tpdu_1_4_1, sizeof(tpdu_1_4_1) )
        << 0x0000       // Command performed successfully
        << QString( "The address data object holds the RP_Destination_Address" )
        << QString( "+112233445566778" )
        << true         // Packing flag
        << 0 << false   // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::NoPduOptions );

    static unsigned char const data_1_5_1[] =
        {0xD0, 0x81, 0xE9, 0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x81, 0x83,
         0x85, 0x38, 0x54, 0x68, 0x65, 0x20, 0x61, 0x64, 0x64, 0x72, 0x65, 0x73,
         0x73, 0x20, 0x64, 0x61, 0x74, 0x61, 0x20, 0x6F, 0x62, 0x6A, 0x65, 0x63,
         0x74, 0x20, 0x68, 0x6F, 0x6C, 0x64, 0x73, 0x20, 0x74, 0x68, 0x65, 0x20,
         0x52, 0x50, 0x20, 0x44, 0x65, 0x73, 0x74, 0x69, 0x6E, 0x61, 0x74, 0x69,
         0x6F, 0x6E, 0x20, 0x41, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x86, 0x09,
         0x91, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0xF8, 0x8B, 0x81, 0x98,
         0x01, 0x00, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF0, 0xA0,
         0xD4, 0xFB, 0x1B, 0x44, 0xCF, 0xC3, 0xCB, 0x73, 0x50, 0x58, 0x5E, 0x06,
         0x91, 0xCB, 0xE6, 0xB4, 0xBB, 0x4C, 0xD6, 0x81, 0x5A, 0xA0, 0x20, 0x68,
         0x8E, 0x7E, 0xCB, 0xE9, 0xA0, 0x76, 0x79, 0x3E, 0x0F, 0x9F, 0xCB, 0x20,
         0xFA, 0x1B, 0x24, 0x2E, 0x83, 0xE6, 0x65, 0x37, 0x1D, 0x44, 0x7F, 0x83,
         0xE8, 0xE8, 0x32, 0xC8, 0x5D, 0xA6, 0xDF, 0xDF, 0xF2, 0x35, 0x28, 0xED,
         0x06, 0x85, 0xDD, 0xA0, 0x69, 0x73, 0xDA, 0x9A, 0x56, 0x85, 0xCD, 0x24,
         0x15, 0xD4, 0x2E, 0xCF, 0xE7, 0xE1, 0x73, 0x99, 0x05, 0x7A, 0xCB, 0x41,
         0x61, 0x37, 0x68, 0xDA, 0x9C, 0xB6, 0x86, 0xCF, 0x66, 0x33, 0xE8, 0x24,
         0x82, 0xDA, 0xE5, 0xF9, 0x3C, 0x7C, 0x2E, 0xB3, 0x40, 0x77, 0x74, 0x59,
         0x5E, 0x06, 0xD1, 0xD1, 0x65, 0x50, 0x7D, 0x5E, 0x96, 0x83, 0xC8, 0x61,
         0x7A, 0x18, 0x34, 0x0E, 0xBB, 0x41, 0xE2, 0x32, 0x08, 0x1E, 0x9E, 0xCF,
         0xCB, 0x64, 0x10, 0x5D, 0x1E, 0x76, 0xCF, 0xE1};
    static unsigned char const resp_1_5_1[] =
        {0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
    static unsigned char const tpdu_1_5_1[] =
        {0x01, 0x01, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF0, 0xA0,
         0xD4, 0xFB, 0x1B, 0x44, 0xCF, 0xC3, 0xCB, 0x73, 0x50, 0x58, 0x5E, 0x06,
         0x91, 0xCB, 0xE6, 0xB4, 0xBB, 0x4C, 0xD6, 0x81, 0x5A, 0xA0, 0x20, 0x68,
         0x8E, 0x7E, 0xCB, 0xE9, 0xA0, 0x76, 0x79, 0x3E, 0x0F, 0x9F, 0xCB, 0x20,
         0xFA, 0x1B, 0x24, 0x2E, 0x83, 0xE6, 0x65, 0x37, 0x1D, 0x44, 0x7F, 0x83,
         0xE8, 0xE8, 0x32, 0xC8, 0x5D, 0xA6, 0xDF, 0xDF, 0xF2, 0x35, 0x28, 0xED,
         0x06, 0x85, 0xDD, 0xA0, 0x69, 0x73, 0xDA, 0x9A, 0x56, 0x85, 0xCD, 0x24,
         0x15, 0xD4, 0x2E, 0xCF, 0xE7, 0xE1, 0x73, 0x99, 0x05, 0x7A, 0xCB, 0x41,
         0x61, 0x37, 0x68, 0xDA, 0x9C, 0xB6, 0x86, 0xCF, 0x66, 0x33, 0xE8, 0x24,
         0x82, 0xDA, 0xE5, 0xF9, 0x3C, 0x7C, 0x2E, 0xB3, 0x40, 0x77, 0x74, 0x59,
         0x5E, 0x06, 0xD1, 0xD1, 0x65, 0x50, 0x7D, 0x5E, 0x96, 0x83, 0xC8, 0x61,
         0x7A, 0x18, 0x34, 0x0E, 0xBB, 0x41, 0xE2, 0x32, 0x08, 0x1E, 0x9E, 0xCF,
         0xCB, 0x64, 0x10, 0x5D, 0x1E, 0x76, 0xCF, 0xE1};
    QTest::newRow( "SEND SHORT MESSAGE 1.5.1 - GCF 27.22.4.10.1" )
        << QByteArray( (char *)data_1_5_1, sizeof(data_1_5_1) )
        << QByteArray( (char *)resp_1_5_1, sizeof(resp_1_5_1) )
        << QByteArray( (char *)tpdu_1_5_1, sizeof(tpdu_1_5_1) )
        << 0x0000       // Command performed successfully
        << QString( "The address data object holds the RP Destination Address" )
        << QString( "+112233445566778" )
        << false        // Packing flag
        << 0 << false   // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::NoPduOptions );

    static unsigned char const data_1_6_1[] =
        {0xD0, 0x81, 0xFD, 0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x81, 0x83,
         0x85, 0x81, 0xE6, 0x54, 0x77, 0x6F, 0x20, 0x74, 0x79, 0x70, 0x65, 0x73,
         0x20, 0x61, 0x72, 0x65, 0x20, 0x64, 0x65, 0x66, 0x69, 0x6E, 0x65, 0x64,
         0x3A, 0x20, 0x2D, 0x20, 0x41, 0x20, 0x73, 0x68, 0x6F, 0x72, 0x74, 0x20,
         0x6D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x20, 0x74, 0x6F, 0x20, 0x62,
         0x65, 0x20, 0x73, 0x65, 0x6E, 0x74, 0x20, 0x74, 0x6F, 0x20, 0x74, 0x68,
         0x65, 0x20, 0x6E, 0x65, 0x74, 0x77, 0x6F, 0x72, 0x6B, 0x20, 0x69, 0x6E,
         0x20, 0x61, 0x6E, 0x20, 0x53, 0x4D, 0x53, 0x2D, 0x53, 0x55, 0x42, 0x4D,
         0x49, 0x54, 0x20, 0x6D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x2C, 0x20,
         0x6F, 0x72, 0x20, 0x61, 0x6E, 0x20, 0x53, 0x4D, 0x53, 0x2D, 0x43, 0x4F,
         0x4D, 0x4D, 0x41, 0x4E, 0x44, 0x20, 0x6D, 0x65, 0x73, 0x73, 0x61, 0x67,
         0x65, 0x2C, 0x20, 0x77, 0x68, 0x65, 0x72, 0x65, 0x20, 0x74, 0x68, 0x65,
         0x20, 0x75, 0x73, 0x65, 0x72, 0x20, 0x64, 0x61, 0x74, 0x61, 0x20, 0x63,
         0x61, 0x6E, 0x20, 0x62, 0x65, 0x20, 0x70, 0x61, 0x73, 0x73, 0x65, 0x64,
         0x20, 0x74, 0x72, 0x61, 0x6E, 0x73, 0x70, 0x61, 0x72, 0x65, 0x6E, 0x74,
         0x6C, 0x79, 0x3B, 0x20, 0x2D, 0x20, 0x41, 0x20, 0x73, 0x68, 0x6F, 0x72,
         0x74, 0x20, 0x6D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x20, 0x74, 0x6F,
         0x20, 0x62, 0x65, 0x20, 0x73, 0x65, 0x6E, 0x74, 0x20, 0x74, 0x6F, 0x20,
         0x74, 0x68, 0x65, 0x20, 0x6E, 0x65, 0x74, 0x77, 0x6F, 0x72, 0x6B, 0x20,
         0x69, 0x6E, 0x20, 0x61, 0x6E, 0x20, 0x53, 0x4D, 0x53, 0x2D, 0x53, 0x55,
         0x42, 0x4D, 0x49, 0x54, 0x20, 0x8B, 0x09, 0x01, 0x00, 0x02, 0x91, 0x10,
         0x40, 0xF0, 0x01, 0x20};
    static unsigned char const resp_1_6_1[] =
        {0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
    static unsigned char const tpdu_1_6_1[] =
        {0x01, 0x01, 0x02, 0x91, 0x10, 0x40, 0xF0, 0x01, 0x20};
    QTest::newRow( "SEND SHORT MESSAGE 1.6.1 - GCF 27.22.4.10.1" )
        << QByteArray( (char *)data_1_6_1, sizeof(data_1_6_1) )
        << QByteArray( (char *)resp_1_6_1, sizeof(resp_1_6_1) )
        << QByteArray( (char *)tpdu_1_6_1, sizeof(tpdu_1_6_1) )
        << 0x0000       // Command performed successfully
        << QString( "Two types are defined: - A short message to be sent to the network "
                    "in an SMS-SUBMIT message, or an SMS-COMMAND message, where the user "
                    "data can be passed transparently; - A short message to be sent to the "
                    "network in an SMS-SUBMIT " )
        << QString( "" )
        << false        // Packing flag
        << 0 << false   // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::NoPduOptions );

    static unsigned char const data_1_7_1[] =
        {0xD0, 0x30, 0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x81, 0x83, 0x85,
         0x00, 0x86, 0x09, 0x91, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0xF8,
         0x8B, 0x18, 0x01, 0x00, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40,
         0xF4, 0x0C, 0x54, 0x65, 0x73, 0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61,
         0x67, 0x65};
    static unsigned char const resp_1_7_1[] =
        {0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
    static unsigned char const tpdu_1_7_1[] =
        {0x01, 0x01, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C,
         0x54, 0x65, 0x73, 0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65};
    QTest::newRow( "SEND SHORT MESSAGE 1.7.1 - GCF 27.22.4.10.1" )
        << QByteArray( (char *)data_1_7_1, sizeof(data_1_7_1) )
        << QByteArray( (char *)resp_1_7_1, sizeof(resp_1_7_1) )
        << QByteArray( (char *)tpdu_1_7_1, sizeof(tpdu_1_7_1) )
        << 0x0000       // Command performed successfully
        << QString( "" )
        << QString( "+112233445566778" )
        << false        // Packing flag
        << 0 << false   // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::EncodeEmptyStrings );

    static unsigned char const data_1_8_1[] =
        {0xD0, 0x2E, 0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x81, 0x83, 0x86,
         0x09, 0x91, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0xF8, 0x8B, 0x18,
         0x01, 0x00, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C,
         0x54, 0x65, 0x73, 0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65};
    static unsigned char const resp_1_8_1[] =
        {0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
    static unsigned char const tpdu_1_8_1[] =
        {0x01, 0x01, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C,
         0x54, 0x65, 0x73, 0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65};
    QTest::newRow( "SEND SHORT MESSAGE 1.8.1 - GCF 27.22.4.10.1" )
        << QByteArray( (char *)data_1_8_1, sizeof(data_1_8_1) )
        << QByteArray( (char *)resp_1_8_1, sizeof(resp_1_8_1) )
        << QByteArray( (char *)tpdu_1_8_1, sizeof(tpdu_1_8_1) )
        << 0x0000       // Command performed successfully
        << QString( "" )
        << QString( "+112233445566778" )
        << false        // Packing flag
        << 0 << false   // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::NoPduOptions );

    static unsigned char const data_2_1_1[] =
        {0xD0, 0x43, 0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x81, 0x83, 0x85,
         0x07, 0x53, 0x65, 0x6E, 0x64, 0x20, 0x53, 0x4D, 0x86, 0x09, 0x91, 0x11,
         0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0xF8, 0x8B, 0x24, 0x01, 0x00, 0x09,
         0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0x08, 0x18, 0x04, 0x17, 0x04,
         0x14, 0x04, 0x20, 0x04, 0x10, 0x04, 0x12, 0x04, 0x21, 0x04, 0x22, 0x04,
         0x12, 0x04, 0x23, 0x04, 0x19, 0x04, 0x22, 0x04, 0x15};
    static unsigned char const resp_2_1_1[] =
        {0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
    static unsigned char const tpdu_2_1_1[] =
        {0x01, 0x01, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0x08, 0x18,
         0x04, 0x17, 0x04, 0x14, 0x04, 0x20, 0x04, 0x10, 0x04, 0x12, 0x04, 0x21,
         0x04, 0x22, 0x04, 0x12, 0x04, 0x23, 0x04, 0x19, 0x04, 0x22, 0x04, 0x15};
    QTest::newRow( "SEND SHORT MESSAGE 2.1.1 - GCF 27.22.4.10.2" )
        << QByteArray( (char *)data_2_1_1, sizeof(data_2_1_1) )
        << QByteArray( (char *)resp_2_1_1, sizeof(resp_2_1_1) )
        << QByteArray( (char *)tpdu_2_1_1, sizeof(tpdu_2_1_1) )
        << 0x0000       // Command performed successfully
        << QString( "Send SM" )
        << QString( "+112233445566778" )
        << false        // Packing flag
        << 0 << false   // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::NoPduOptions );

    static unsigned char const data_3_1_1a[] =
        {0xD0, 0x3B, 0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x81, 0x83, 0x85,
         0x07, 0x4E, 0x4F, 0x20, 0x49, 0x43, 0x4F, 0x4E, 0x86, 0x09, 0x91, 0x11,
         0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0xF8, 0x8B, 0x18, 0x01, 0x00, 0x09,
         0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C, 0x54, 0x65, 0x73,
         0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x9E, 0x02, 0x00,
         0x01};
    static unsigned char const resp_3_1_1a[] =
        {0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
    static unsigned char const tpdu_3_1_1a[] =
        {0x01, 0x01, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C,
         0x54, 0x65, 0x73, 0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65};
    QTest::newRow( "SEND SHORT MESSAGE 3.1.1A - GCF 27.22.4.10.3" )
        << QByteArray( (char *)data_3_1_1a, sizeof(data_3_1_1a) )
        << QByteArray( (char *)resp_3_1_1a, sizeof(resp_3_1_1a) )
        << QByteArray( (char *)tpdu_3_1_1a, sizeof(tpdu_3_1_1a) )
        << 0x0000       // Command performed successfully
        << QString( "NO ICON" )
        << QString( "+112233445566778" )
        << false        // Packing flag
        << 1 << true    // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::NoPduOptions );

    static unsigned char const data_3_1_1b[] =
        {0xD0, 0x3B, 0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x81, 0x83, 0x85,
         0x07, 0x4E, 0x4F, 0x20, 0x49, 0x43, 0x4F, 0x4E, 0x86, 0x09, 0x91, 0x11,
         0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0xF8, 0x8B, 0x18, 0x01, 0x00, 0x09,
         0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C, 0x54, 0x65, 0x73,
         0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x9E, 0x02, 0x00,
         0x01};
    static unsigned char const resp_3_1_1b[] =
        {0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x04};
    static unsigned char const tpdu_3_1_1b[] =
        {0x01, 0x01, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C,
         0x54, 0x65, 0x73, 0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65};
    QTest::newRow( "SEND SHORT MESSAGE 3.1.1B - GCF 27.22.4.10.3" )
        << QByteArray( (char *)data_3_1_1b, sizeof(data_3_1_1b) )
        << QByteArray( (char *)resp_3_1_1b, sizeof(resp_3_1_1b) )
        << QByteArray( (char *)tpdu_3_1_1b, sizeof(tpdu_3_1_1b) )
        << 0x0004       // Icon not displayed
        << QString( "NO ICON" )
        << QString( "+112233445566778" )
        << false        // Packing flag
        << 1 << true    // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::NoPduOptions );

    static unsigned char const data_3_2_1a[] =
        {0xD0, 0x3B, 0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x81, 0x83, 0x85,
         0x07, 0x53, 0x65, 0x6E, 0x64, 0x20, 0x53, 0x4D, 0x86, 0x09, 0x91, 0x11,
         0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0xF8, 0x8B, 0x18, 0x01, 0x00, 0x09,
         0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C, 0x54, 0x65, 0x73,
         0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x1E, 0x02, 0x01,
         0x01};
    static unsigned char const resp_3_2_1a[] =
        {0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
    static unsigned char const tpdu_3_2_1a[] =
        {0x01, 0x01, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C,
         0x54, 0x65, 0x73, 0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65};
    QTest::newRow( "SEND SHORT MESSAGE 3.2.1A - GCF 27.22.4.10.3" )
        << QByteArray( (char *)data_3_2_1a, sizeof(data_3_2_1a) )
        << QByteArray( (char *)resp_3_2_1a, sizeof(resp_3_2_1a) )
        << QByteArray( (char *)tpdu_3_2_1a, sizeof(tpdu_3_2_1a) )
        << 0x0000       // Command performed successfully
        << QString( "Send SM" )
        << QString( "+112233445566778" )
        << false        // Packing flag
        << 1 << false   // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::NoPduOptions );

    static unsigned char const data_3_2_1b[] =
        {0xD0, 0x3B, 0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x81, 0x83, 0x85,
         0x07, 0x53, 0x65, 0x6E, 0x64, 0x20, 0x53, 0x4D, 0x86, 0x09, 0x91, 0x11,
         0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0xF8, 0x8B, 0x18, 0x01, 0x00, 0x09,
         0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C, 0x54, 0x65, 0x73,
         0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x1E, 0x02, 0x01,
         0x01};
    static unsigned char const resp_3_2_1b[] =
        {0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x04};
    static unsigned char const tpdu_3_2_1b[] =
        {0x01, 0x01, 0x09, 0x91, 0x10, 0x32, 0x54, 0x76, 0xF8, 0x40, 0xF4, 0x0C,
         0x54, 0x65, 0x73, 0x74, 0x20, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65};
    QTest::newRow( "SEND SHORT MESSAGE 3.2.1B - GCF 27.22.4.10.3" )
        << QByteArray( (char *)data_3_2_1b, sizeof(data_3_2_1b) )
        << QByteArray( (char *)resp_3_2_1b, sizeof(resp_3_2_1b) )
        << QByteArray( (char *)tpdu_3_2_1b, sizeof(tpdu_3_2_1b) )
        << 0x0004       // Icon not displayed
        << QString( "Send SM" )
        << QString( "+112233445566778" )
        << false        // Packing flag
        << 1 << false   // Icon details
        << QByteArray() << QString() // No text attribute information
        << (int)( QSimCommand::NoPduOptions );

    // Test only one of the text attribute test cases.  We assume that if
    // one works, then they will all work.  The DISPLAY TEXT command tests
    // the formatting rules.
    static unsigned char const data_4_1_1[] =
        {0xD0, 0x2C, 0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x81, 0x83, 0x85,
         0x10, 0x54, 0x65, 0x78, 0x74, 0x20, 0x41, 0x74, 0x74, 0x72, 0x69, 0x62,
         0x75, 0x74, 0x65, 0x20, 0x31, 0x8B, 0x09, 0x01, 0x00, 0x02, 0x91, 0x10,
         0x40, 0xF0, 0x01, 0x20, 0xD0, 0x04, 0x00, 0x10, 0x00, 0xB4};
    static unsigned char const resp_4_1_1[] =
        {0x81, 0x03, 0x01, 0x13, 0x00, 0x82, 0x02, 0x82, 0x81, 0x83, 0x01, 0x00};
    static unsigned char const tpdu_4_1_1[] =
        {0x01, 0x01, 0x02, 0x91, 0x10, 0x40, 0xF0, 0x01, 0x20};
    static unsigned char const attr_4_1_1[] =
        {0x00, 0x10, 0x00, 0xB4};
    QTest::newRow( "SEND SHORT MESSAGE 4.1.1 - GCF 27.22.4.10.4" )
        << QByteArray( (char *)data_4_1_1, sizeof(data_4_1_1) )
        << QByteArray( (char *)resp_4_1_1, sizeof(resp_4_1_1) )
        << QByteArray( (char *)tpdu_4_1_1, sizeof(tpdu_4_1_1) )
        << 0x0000       // Command performed successfully
        << QString( "Text Attribute 1" )
        << QString( "" )
        << false        // Packing flag
        << 0 << false   // Icon details
        << QByteArray( (char *)attr_4_1_1, sizeof(attr_4_1_1) )
        << QString( "<body bgcolor=\"#FFFF00\"><div align=\"left\"><font color=\"#008000\">Text Attribute 1</font></div></body>" )
        << (int)( QSimCommand::NoPduOptions );
}