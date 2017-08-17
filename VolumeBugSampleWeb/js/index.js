
var nativePlayer;
var systemMediaControls;
var loadedUrl

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

    document.getElementById('btnLoad').addEventListener('click', function () {
        loadedUrl = $('#txtUrl').val();
        playback_controls.enable();
    });

    document.getElementById('btnPlay').addEventListener('click', function () {
        playMedia(loadedUrl);
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
        var shallMute = !$('#btnMute').hasClass('active')
        if (shallMute)
            mute();
        else
            unmute();
    });

});

function playMedia(url) {
    if (nativePlayer) {
        try {
            nativePlayer.initialize(url);

            // update the ui - buttons and progress bar
            $('#btnPlay').prop('disabled', true);
            $('#btnStop').prop('disabled', false);
            $('#btnVolInc').prop('disabled', false);
            $('#btnVolDec').prop('disabled', false);
            $('#btnMute').prop('disabled', false);
            $('#progressbar').css('width', '100%');

            systemMediaControls.isEnabled = true;
            systemMediaControls.playbackStatus = Windows.Media.MediaPlaybackStatus.playing;

            // update the SMTC 
            var updater = systemMediaControls.displayUpdater;
            updater.type = Windows.Media.MediaPlaybackType.music;
            updater.musicProperties.artist = "Dummy Artist";
            updater.musicProperties.albumArtist = "Dummy Album Artist";
            updater.musicProperties.title = "Dummy title";
            updater.thumbnail = Windows.Storage.Streams.RandomAccessStreamReference.createFromUri(new Windows.Foundation.Uri('ms-appx:///artwork/artwork_198x192.jpg'));

            updater.update();
        }
        catch (err) {
            bootstrap_alert.warning(err.message);
            playback_controls.disable();
        }
    }
}

function stopMedia() {
    if (nativePlayer) {
        try {
            // update the ui - buttons and progress bar
            $('#btnStop').prop('disabled', true);
            $('#btnVolInc').prop('disabled', true);
            $('#btnVolDec').prop('disabled', true);
            $('#btnMute').prop('disabled', true);
            $('#btnPlay').prop('disabled', false);

            $('#progressbar').css('width', '0%');

            // in case it is muted unmute it....
            nativePlayer.unmute();
            nativePlayer.shutdown();

            systemMediaControls.playbackStatus = Windows.Media.MediaPlaybackStatus.closed;
        }
        catch (err) {
            bootstrap_alert.warning(err.message);
            playback_controls.disable();
        }
    }
}

function increaseVolume() {
    if (nativePlayer) {
        try {
            nativePlayer.volumeIncrease();
        }
        catch (err) {
            bootstrap_alert.warning(err.message);
        }

    }
}

function decreaseVolume() {
    if (nativePlayer) {
        try {
            nativePlayer.volumeDecrease();
        }
        catch (err) {
            bootstrap_alert.warning(err.message);
        }
    }
}

function mute() {
    if (nativePlayer) {
        try {
            nativePlayer.mute();
        }
        catch (err) {
            bootstrap_alert.warning(err.message);
        }
    }
}

function unmute() {
    if (nativePlayer) {
        try {
            nativePlayer.unmute();
        }
        catch (err) {
            bootstrap_alert.warning(err.message);
        }
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
            playMedia(loadedUrl);
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

bootstrap_alert = function () { }
bootstrap_alert.warning = function (message) {
    $('#alert_placeholder').html('<div class="alert alert-warning alert-dismissible fade show" role="alert"><button type="button" class="close" data-dismiss="alert" aria-hidden="true">&times;</button><span>Error: ' + message + '</span></div>')
}

playback_controls = function () { }
playback_controls.enable = function () {
    $('#controlPanel').removeClass('d-none');
    $('#btnPlay').prop('disabled', false);
}
playback_controls.disable = function () {
    $('#controlPanel').addClass('d-none');
    $('#btnPlay').prop('disabled', true);
}
