
static const char *xcb_input_device_type_str(
		xcb_input_device_type_t type)
{
	switch (type) {
	case XCB_INPUT_DEVICE_TYPE_MASTER_POINTER: return "master pointer";
	case XCB_INPUT_DEVICE_TYPE_MASTER_KEYBOARD: return "master keyboard";
	case XCB_INPUT_DEVICE_TYPE_SLAVE_POINTER: return "slave pointer";
	case XCB_INPUT_DEVICE_TYPE_SLAVE_KEYBOARD: return "slave keyboard";
	case XCB_INPUT_DEVICE_TYPE_FLOATING_SLAVE: return "floating slave";
	}
	return "unknown";
}

static const char *xcb_input_device_class_type_str(
		xcb_input_device_class_type_t class)
{
	switch (class) {
	case XCB_INPUT_DEVICE_CLASS_TYPE_KEY: return "key";
	case XCB_INPUT_DEVICE_CLASS_TYPE_BUTTON: return "button";
	case XCB_INPUT_DEVICE_CLASS_TYPE_VALUATOR: return "valuator";
	case XCB_INPUT_DEVICE_CLASS_TYPE_SCROLL: return "scroll";
	case XCB_INPUT_DEVICE_CLASS_TYPE_TOUCH: return "touch";
	}
	return "unknown";
}

static char *input_lsdevs_label(xcb_atom_t label)
{
	char *rval;
	xcb_generic_error_t *e = NULL;

	xcb_get_atom_name_cookie_t nc = xcb_get_atom_name(xcb_con, label);
	xcb_get_atom_name_reply_t *nr = xcb_get_atom_name_reply(xcb_con, nc, &e);
	assert(e == NULL);
	int len = xcb_get_atom_name_name_length(nr);
	char *from = xcb_get_atom_name_name(nr);
	rval = memcpy(malloc(len + 1), from, len);
	rval[len] = '\0';
	free(nr);
	return rval;
}

static void input_lsdevs_class(xcb_input_device_id_t devid,
		xcb_input_device_class_t *c)
{
	if (c->type == XCB_INPUT_DEVICE_CLASS_TYPE_VALUATOR) {
		xcb_input_valuator_class_t *vc
			= (xcb_input_valuator_class_t *) c;
		char *label = input_lsdevs_label(vc->label);
		lprintf(TXT "\t"lBLD_"%9s"_lBLD
				" len%3i(%i) srcid%3i nr%3i label\"%s\"\n",
				"valuator", c->len, sizeof(vc[0]) >> 2,
				c->sourceid, vc->number, label);
		free(label);
	} else if (c->type == XCB_INPUT_DEVICE_CLASS_TYPE_BUTTON) {
		xcb_input_button_class_t *bc
			= (xcb_input_button_class_t *) c;
		lprintf(TXT "\t"lBLD_"%9s"_lBLD" len%3i(%i) srcid%3i "
				"num_btns%3i\n",
				"button", c->len, sizeof(bc[0]) >> 2,
				c->sourceid, bc->num_buttons);
		xcb_atom_t *btnlabels = xcb_input_button_class_labels(bc);
		int btnlabels_len = xcb_input_button_class_labels_length(bc);
		for (int i = 0; i < btnlabels_len; i++) {
			if (btnlabels[i] == 0) /* unnamed and unimportant */
				continue;
			char *label = input_lsdevs_label(btnlabels[i]);
			lprintf(TXT "\t\t%i :: \"%s\"\n", i, label);
			free(label);
		}
	} else {
		lprintf(TXT "\t"lBLD_"%9s"_lBLD" len%3i srcid%3i\n",
				xcb_input_device_class_type_str(c->type),
				c->len, c->sourceid);
	}
}

static void input_lsdevs()
{
	assert(xcb_con != NULL);

	/* XCB_INPUT_DEVICE_ALL == XIAllDevices */
	xcb_input_xi_query_device_cookie_t c
		= xcb_input_xi_query_device(xcb_con, XCB_INPUT_DEVICE_ALL);
	xcb_generic_error_t *e = NULL;
	xcb_input_xi_query_device_reply_t *r
		= xcb_input_xi_query_device_reply(xcb_con, c, &e);
	assert(e == NULL);

	xcb_input_xi_device_info_iterator_t iter
		= xcb_input_xi_query_device_infos_iterator(r);

	while (iter.rem) {
		xcb_input_xi_device_info_t *d = iter.data;
		lprintf(TXT "devid"lBLD_"%3i"_lBLD" "lBLD_"%30.*s"_lBLD
				" : "lBLD_"%s"_lBLD"\n",
				d->deviceid,
				xcb_input_xi_device_info_name_length(d),
				xcb_input_xi_device_info_name(d),
				xcb_input_device_type_str(d->type));

		/* iterate classes */
		xcb_input_device_class_iterator_t cl_iter
			= xcb_input_xi_device_info_classes_iterator(d);
		while (cl_iter.rem) {
			input_lsdevs_class(d->deviceid, cl_iter.data);
			xcb_input_device_class_next(&cl_iter);
		}

		xcb_input_xi_device_info_next(&iter);
	}

	free(r);
}

