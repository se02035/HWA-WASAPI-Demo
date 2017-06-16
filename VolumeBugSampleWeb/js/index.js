
var nativePlayer;
var systemMediaControls;

document.addEventListener("DOMContentLoaded", function (event) {

    if (typeof Windows !== 'undefined') {
        nativePlayer = new NativePlayerComponent.SamplePlayer();

        setupSystemMediaTransportControls();

        //// system id
        //var idAsBuffer = Windows.System.Profile.SystemIdentification.getSystemIdForPublisher().id;
        //var dataReader = Windows.Storage.Streams.DataReader.fromBuffer(idAsBuffer);
        //var bytes = new Uint8Array(idAsBuffer.length);
        //dataReader.readBytes(bytes);
        //var result = HelperComponent.Helper.convertToString(bytes);
    }

    document.getElementById('btnPlay').addEventListener('click', function () {
        playMedia();
    });

    document.getElementById('btnStop').addEventListener('click', function () {
        stopMedia();
    });

    document.getElementById('btnVolInc').addEventListener('click', function () {
        increaseVolume();
    });

    document.getElementById('btnVolDec').addEventListener('click', function () {
        decreaseVolume();
    });

    document.getElementById('btnMute').addEventListener('click', function () {
        mute();
    });

    document.getElementById('btnUnmute').addEventListener('click', function () {
        unmute();
    });

});

function playMedia() {
    if (nativePlayer) {
        nativePlayer.initialize();
        systemMediaControls.isEnabled = true;
        systemMediaControls.playbackStatus = Windows.Media.MediaPlaybackStatus.playing;
    }
}

function stopMedia() {
    if (nativePlayer) {
        nativePlayer.shutdown();
        systemMediaControls.playbackStatus = Windows.Media.MediaPlaybackStatus.closed;
    }
}

function increaseVolume() {
    if (nativePlayer) {
        nativePlayer.volumeIncrease();
    }
}

function decreaseVolume() {
    if (nativePlayer) {
        nativePlayer.volumeDecrease();
    }
}

function mute() {
    if (nativePlayer) {
        nativePlayer.mute();
    }
}

function unmute() {
    if (nativePlayer) {
        nativePlayer.unmute();
    }
}

function setupSystemMediaTransportControls() {
    systemMediaControls = Windows.Media.SystemMediaTransportControls.getForCurrentView();
    systemMediaControls.isEnabled = false;

    systemMediaControls.addEventListener("buttonpressed", systemMediaControlsButtonPressed, false);
    systemMediaControls.addEventListener("propertychanged", systemMediaControlsPropertyChanged, false);

    systemMediaControls.addEventListener("playbackratechangerequested", systemMediaControlsPlaybackRateChangeRequested, false);
    systemMediaControls.addEventListener("playbackpositionchangerequested", systemMediaControlsPlaybackPositionChangeRequested, false);
    systemMediaControls.addEventListener("autorepeatmodechangerequested", systemMediaControlsAutoRepeatModeChangeRequested, false);

    systemMediaControls.isPlayEnabled = true;
    systemMediaControls.isPauseEnabled = true;
    systemMediaControls.isStopEnabled = true;
    systemMediaControls.playbackStatus = Windows.Media.MediaPlaybackStatus.closed;
}

function systemMediaControlsButtonPressed(args) {
    switch (args.button) {
        case Windows.Media.SystemMediaTransportControlsButton.play:
            playMedia();
            break;
        case Windows.Media.SystemMediaTransportControlsButton.pause:
            stopMedia();
            break;
        default:
    }
}

function systemMediaControlsPropertyChanged() {

}

function systemMediaControlsPlaybackRateChangeRequested() {

}

function systemMediaControlsPlaybackPositionChangeRequested() {

}

function systemMediaControlsAutoRepeatModeChangeRequested() {

}